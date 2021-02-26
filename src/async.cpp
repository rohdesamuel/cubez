#include <cubez/async.h>

#include <cubez/memory.h>
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <list>
#include <mutex>
#include <ctime>
#include <unordered_map>

#include "defs.h"
#include "thread_pool.h"

TaskThreadPool* thread_pool;

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

void qb_channel_create(qbChannel* channel) {
  *channel = new qbChannel_{};
}

qbResult qb_channel_write(qbChannel channel, qbVar v) {
  channel->write(v);
  return QB_OK;
}

qbResult qb_channel_read(qbChannel channel, qbVar* v) {
  channel->read(v);
  return QB_OK;
}

qbVar qb_channel_select(qbChannel* channels, uint8_t len) {
  std::condition_variable cv;
  std::mutex mx;

  uint8_t* channel_indices = (uint8_t*)alloca(len);
  for (auto i = 0; i < len; ++i) {
    channels[i]->set_select_condition(&cv, &mx);
    channel_indices[i] = i;
  }

  // To ensure no starvation when reading, iterate through in a random order.
  std::random_shuffle(channel_indices, channel_indices + len);

  qbVar ret = qbNil;
  qbChannel selected;
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

void async_initialize(qbSchedulerAttr_* attr) {
  thread_pool = new TaskThreadPool(attr ? attr->max_async_tasks : 8);
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

qbChannel qb_task_input(qbTask task, uint8_t input) {
  return thread_pool->input(task->task_id, input);
}

qbVar qb_task_join(qbTask task) {
  return thread_pool->join(task);
}