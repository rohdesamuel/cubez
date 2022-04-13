#include <cubez/async.h>

#include <cubez/memory.h>
#include <cubez/random.h>
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
#include "async_internal.h"

TaskThreadPool* thread_pool;

thread_local std::random_device* rd;

// Reserved threads:
// 1. log.cpp
constexpr uint64_t QB_RESERVED_SYSTEM_THREADS = 1;

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
public:
  struct State {
    std::unordered_map<qbTaskBundle, std::vector<qbSemaphoreWaitInfo_>> semaphores;
    std::unordered_map<qbTaskBundle, qbTask> tasks;
  };

  qbTaskBundleBeginInfo_ begin_info;
  std::vector<std::function<qbVar(qbTask, qbVar)>> tasks_;

  void add_bundle(qbTaskBundle bundle, qbTask task) {
    std::shared_lock l(state_mu_);
    State& state = state_.find(task)->second;
    state.tasks[bundle] = task;
  }

  void add_semaphores(qbTask task, size_t count, qbTaskBundleSemaphore* semaphores) {
    State* state;
    {
      std::shared_lock l(state_mu_);
      state = &state_[task];
    }

    for (size_t i = 0; i < count; ++i) {
      qbTaskBundleSemaphore s = semaphores[i];
      state->semaphores[s->task_bundle].push_back(s->wait_info);
    }    
  }

  qbVar join_bundle(qbTask task, qbTaskBundle bundle) {
    State* state;
    {
      std::shared_lock l(state_mu_);
      state = &state_[task];
    }
    qbTask async_task = state->tasks.find(bundle)->second;

    auto found = state->semaphores.find(bundle);
    if (found != state->semaphores.end()) {
      for (const auto& s : found->second) {
        qb_semaphore_wait(s.semaphore, s.wait_until);
      }
    }

    return qb_task_join(async_task);
  }

  void join_all_bundles(qbTask task) {
    State* state;
    {
      std::shared_lock l(state_mu_);
      state = &state_[task];
    }
    
    for (auto& [bundle, async_task] : state->tasks) {
      auto found = state->semaphores.find(bundle);
      if (found != state->semaphores.end()) {
        for (const auto& s : found->second) {
          qb_semaphore_wait(s.semaphore, s.wait_until);
        }
      }

      qb_task_join(async_task);
    }
  }

  void clear_state(qbTask task) {
    std::shared_lock l(state_mu_);
    state_.erase(task);
  }

private:
  std::shared_mutex state_mu_;
  std::unordered_map<qbTask, State> state_;
};

struct qbSemaphore_ {
  std::mutex mu;
  std::condition_variable cv;
  uint64_t n = 0;
};

void qb_semaphore_create(qbSemaphore* sem) {
  *sem = new qbSemaphore_{};
}

void qb_semaphore_destroy(qbSemaphore* sem) {
  delete *sem;
  *sem = nullptr;
}

qbResult qb_semaphore_signal(qbSemaphore sem, uint64_t n) {
  if (n < sem->n)
    return QB_ERROR_SEMAPHORE_NONMONOTONIC_SIGNAL;

  {
    std::unique_lock<std::mutex> l(sem->mu);
    sem->n = n;
  }
  sem->cv.notify_all();
  return QB_OK;
}

void qb_semaphore_wait(qbSemaphore sem, uint64_t n) {
  std::unique_lock l(sem->mu);
  sem->cv.wait(l, [sem, n] { return sem->n >= n; });
}

void qb_semaphore_reset(qbSemaphore sem) {
  std::unique_lock<std::mutex> l(sem->mu);
  sem->n = 0;
}

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
  max_async_tasks += QB_RESERVED_SYSTEM_THREADS;
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

void qb_taskbundle_end(qbTaskBundle bundle) {}

void qb_taskbundle_clear(qbTaskBundle bundle) {
  bundle->tasks_.resize(0);
}

void qb_taskbundle_addtask(qbTaskBundle bundle, qbVar(*entry)(qbTask, qbVar), qbTaskBundleAddTaskInfo info) {
  bundle->tasks_.push_back([entry](qbTask task, qbVar var) {
    return entry(task, var);
  });
}

void qb_taskbundle_addtask(qbTaskBundle bundle, std::function<qbVar(qbTask, qbVar)>&& entry, qbTaskBundleAddTaskInfo info) {
  bundle->tasks_.push_back(std::move(entry));
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
  bundle->tasks_.push_back([bundle, tasks, submit_info](qbTask task, qbVar var) {
    qbTask async_task = qb_taskbundle_submit(bundle, var, submit_info);
    bundle->add_bundle(tasks, async_task);
    return var;
  });
}

void qb_taskbundle_joinbundle(qbTaskBundle bundle, qbTaskBundle tasks, qbTaskBundleAddTaskInfo add_info) {
  bundle->tasks_.push_back([bundle, tasks](qbTask task, qbVar var) {
    return bundle->join_bundle(task, tasks);
  });
}

void qb_taskbundle_addsleep(qbTaskBundle bundle, uint64_t duration_ms, qbTaskBundleAddTaskInfo info) {
  bundle->tasks_.push_back([duration_ms](qbTask, qbVar var) {
    std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
    return var;
  });
}

qbTask qb_taskbundle_submit(qbTaskBundle bundle, qbVar arg, qbTaskBundleSubmitInfo info) {
  qbTask task = thread_pool->enqueue([bundle, task, arg, info]() {
    qbVar varg = arg;

    if (info) {
      bundle->add_semaphores(task, info->semaphore_count, info->semaphores);
    }

    for (auto& entry : bundle->tasks_) {
      varg = entry(task, varg);
    }
    
    bundle->join_all_bundles(task);
    bundle->clear_state(task);

    return varg;
  });
  return task;
}

qbTask qb_taskbundle_dispatch(qbTaskBundle bundle, qbVar arg, qbTaskBundleSubmitInfo info) {
  return thread_pool->dispatch([bundle, arg, info](qbTask task) {
    qbVar varg = arg;

    if (info) {
      bundle->add_semaphores(task, info->semaphore_count, info->semaphores);
    }

    for (auto& entry : bundle->tasks_) {
      varg = entry(task, varg);
    }

    bundle->join_all_bundles(task);
    bundle->clear_state(task);
    
    return varg;
  });
}

qbVar qb_taskbundle_run(qbTaskBundle bundle, qbVar arg, qbTaskBundleSubmitInfo info) {
  qbTask task = qb_rand();

  if (info) {
    bundle->add_semaphores(task, info->semaphore_count, info->semaphores);
  }

  for (auto& entry : bundle->tasks_) {
    arg = entry(task, arg);
  }

  bundle->join_all_bundles(task);
  bundle->clear_state(task);

  return arg;
}