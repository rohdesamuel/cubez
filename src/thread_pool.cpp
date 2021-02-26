#include "thread_pool.h"

// the constructor just launches some amount of workers
TaskThreadPool::TaskThreadPool(size_t threads)
  : stop_(false), max_inputs_(QB_TASK_MAX_NUM_INPUTS) {
  for (qbId task_id = 0; task_id < threads; ++task_id) {
    tasks_cvs_.push_back(new std::condition_variable);
    tasks_join_mutexes_.push_back(new std::mutex);

    task_inputs_.push_back({});
    for (uint8_t i = 0; i < max_inputs_; ++i) {
      qbChannel channel;
      qb_channel_create(&channel);
      task_inputs_[task_id].push_back(channel);
    }
  }

  for (qbId task_id = 0; task_id < threads; ++task_id) {
    workers_.emplace_back(
      [this, task_id] {
      for (;;) {
        std::function<void(qbId)> task;

        {
          std::unique_lock<std::mutex> lock(this->queue_mutex_);
          this->condition_.wait(lock,
                                [this] { return this->stop_ || !this->tasks_.empty(); });
          if (this->stop_ && this->tasks_.empty())
            return;
          task = std::move(this->tasks_.front());
          this->tasks_.pop();
        }

        {          
          std::unique_lock<std::mutex> l(*this->tasks_join_mutexes_[task_id]);
          task(task_id);          
        }
      }
    });
  }
}

// the destructor joins all threads
TaskThreadPool::~TaskThreadPool() {
  {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    stop_ = true;
  }
  condition_.notify_all();
  for (std::thread &worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

qbVar TaskThreadPool::join(qbTask task) {
  std::unique_lock<std::mutex> l(*this->tasks_join_mutexes_[task->task_id]);
  return task->output;
}

qbChannel TaskThreadPool::input(qbId task_id, uint8_t input) {
  if (task_id >= workers_.size()) {
    return nullptr;
  }

  if (input >= QB_TASK_MAX_NUM_INPUTS) {
    return nullptr;
  }

  return task_inputs_[task_id][input];
}