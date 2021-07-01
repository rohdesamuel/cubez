#include <cubez/async.h>

#include <cubez/memory.h>
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <list>
#include <mutex>
#include <ctime>
#include <unordered_map>
#include <random>

#include "concurrentqueue.h"
#include "defs.h"
#include "thread_pool.h"

TaskThreadPool* thread_pool;

thread_local std::random_device* rd;

struct qbChannel_ {
public:
  std::atomic_bool data_avail = false;

  void write(qbVar v) {
    std::unique_lock<std::mutex> l(buffer_mutex_);
    if (select_condition_) {
      select_mutex_->lock();
    }

    qbVar copy = qbNil;
    qb_var_copy(&v, &copy);

    buffer_.push_back(copy);
    data_avail = true;
    cv_.notify_all();
    if (select_condition_) {
      select_mutex_->unlock();
      select_condition_->notify_all();
    }    
  }

  void read(qbVar* v) {
    std::lock_guard<decltype(single_reader_mutex_)> single_reader_enforcer(single_reader_mutex_);
    std::unique_lock<std::mutex> l(buffer_mutex_);

    cv_.wait(l, [&]() { return data_avail.load(); });
    *v = buffer_.front();
    buffer_.pop_front();

    if (buffer_.empty()) {
      data_avail = false;
    }
  }

  bool peek(qbVar* v) {
    std::unique_lock<decltype(buffer_mutex_)> l(buffer_mutex_);
    if (data_avail) {
      *v = buffer_.front();
      return true;
    }
    return false;
  }

  void set_select_condition(std::condition_variable* select_condition, std::mutex* select_mutex) {
    std::lock_guard<decltype(single_reader_mutex_)> single_reader_enforcer(single_reader_mutex_);
    std::unique_lock<std::mutex> l(buffer_mutex_);

    select_condition_ = select_condition;
    select_mutex_ = select_mutex;
  }

private:
  size_t elem_size_;
  qbMemoryAllocator allocator_;

  std::list<qbVar> buffer_;
  std::mutex buffer_mutex_;

  std::mutex single_reader_mutex_;

  std::condition_variable cv_;
  std::condition_variable* select_condition_;
  std::mutex* select_mutex_;
};

struct qbQueue_ {
  moodycamel::ConcurrentQueue<qbVar> queue_;
};

struct qbTaskBundle_ {
  qbTaskBundleBeginInfo_ begin_info;
  std::vector<std::function<qbVar(qbTask, qbVar)>> tasks_;
};

void qb_channel_create(qbChannel* channel) {
  *channel = new qbChannel_{};
}

void qb_channel_destroy(qbChannel* channel) {
  delete *channel;
  *channel = nullptr;
}

void qb_channel_write(qbChannel channel, qbVar v) {
  channel->write(v);
}

void qb_channel_read(qbChannel channel, qbVar* v) {
  channel->read(v);
}

qbVar qb_channel_select(qbChannel* channels, uint8_t len) {
  std::condition_variable cv;
  std::mutex mx;

  uint8_t* channel_indices = (uint8_t*)alloca(len);
  for (auto i = 0; i < len; ++i) {
    channels[i]->set_select_condition(&cv, &mx);
    channel_indices[i] = i;
  }

  if (!rd) rd = new std::random_device;

  // To ensure no starvation when reading, iterate through in a random order.
  std::shuffle(channel_indices, channel_indices + len, std::mt19937{});

  qbVar ret = qbNil;
  qbChannel selected = nullptr;
  {
    std::unique_lock<std::mutex> l(mx);
    cv.wait(l, [&]() {
      // Return the first available piece of data.
      for (auto i = 0; i < len; ++i) {
        uint8_t index = channel_indices[i];
        if (channels[index]->data_avail) {
          selected = channels[index];          
          return true;
        }
      }
      return false;
    });
  }

  // Reset the state.
  for (auto i = 0; i < len; ++i) {
    channels[i]->set_select_condition(nullptr, nullptr);
  }

  selected->read(&ret);

  return ret;
}

void qb_queue_create(qbQueue* queue) {
  *queue = new qbQueue_;
}

void qb_queue_destroy(qbQueue* queue) {
  delete *queue;
  *queue = nullptr;
}

void qb_queue_write(qbQueue queue, qbVar v) {
  queue->queue_.enqueue(v);
}

qbBool qb_queue_tryread(qbQueue queue, qbVar* v) {
  return queue->queue_.try_dequeue(*v);
}

void async_initialize(qbSchedulerAttr_* attr) {
  size_t max_async_tasks = attr ? attr->max_async_tasks : std::thread::hardware_concurrency();
  size_t max_async_tasks_queue_size = attr ? attr->max_async_tasks : 1024;
  thread_pool = new TaskThreadPool(max_async_tasks, max_async_tasks_queue_size);
}

void async_stop() {
  delete thread_pool;
}

qbTask qb_task_async(qbVar(*entry)(qbTask, qbVar), qbVar var) {
  qbTask task = thread_pool->enqueue([task, entry, var]() -> qbVar {
    return (*entry)(task, var);
  });
  
  return task;
}

qbVar qb_task_join(qbTask task) {
  return thread_pool->join(task);
}

qbBool qb_task_isactive(qbTask task) {
  return thread_pool->is_active(task);
}

qbTaskBundle qb_taskbundle_create(qbTaskBundleAttr attr) {
  return new qbTaskBundle_;
}

void qb_taskbundle_begin(qbTaskBundle bundle, qbTaskBundleBeginInfo info) {
  if (info) {
    bundle->begin_info = *info;    
  }
  bundle->tasks_.clear();
}

void qb_taskbundle_end(qbTaskBundle bundle) {
}

void qb_taskbundle_addtask(qbTaskBundle bundle, qbVar(*entry)(qbTask, qbVar), qbTaskBundleAddTaskInfo info) {
  bundle->tasks_.push_back([entry](qbTask task, qbVar var) {
    return entry(task, var);
  });
}

void qb_taskbundle_addsystem(qbTaskBundle bundle, qbSystem system, qbTaskBundleAddTaskInfo info) {
  bundle->tasks_.push_back([system](qbTask, qbVar var) {
    return qb_system_run(system, var);
  });
}

void qb_taskbundle_addquery(qbTaskBundle bundle, qbQuery query, qbTaskBundleAddTaskInfo info) {
  bundle->tasks_.push_back([query](qbTask, qbVar var) {
    qb_query(query, var);
    return var;
  });
}

void qb_taskbundle_addbundle(qbTaskBundle bundle, qbTaskBundle tasks,
                             qbTaskBundleAddTaskInfo add_info,
                             qbTaskBundleSubmitInfo submit_info) {
  qbTaskBundleSubmitInfo_ info_copy = *submit_info;
  bundle->tasks_.push_back([bundle, info_copy](qbTask, qbVar var) {
    return qb_task_join(qb_taskbundle_submit(bundle, var, (qbTaskBundleSubmitInfo)&info_copy));
  });
}

qbTask qb_taskbundle_submit(qbTaskBundle bundle, qbVar arg, qbTaskBundleSubmitInfo info) {
  decltype(qbTaskBundle_::tasks_) tasks = std::move(bundle->tasks_);

  qbTask task = thread_pool->enqueue([tasks, task, arg]() {
    qbVar varg = arg;
    for (auto& entry : tasks) {
      varg = entry(task, varg);
    }
    return varg;
  });
  return task;
}

qbTask qb_taskbundle_dispatch(qbTaskBundle bundle, qbVar arg, qbTaskBundleSubmitInfo info) {
  decltype(qbTaskBundle_::tasks_) tasks = std::move(bundle->tasks_);

  return thread_pool->dispatch([tasks, arg](qbTask task) {
    qbVar varg = arg;
    for (auto& entry : tasks) {
      varg = entry(task, varg);
    }
    return varg;
  });
}