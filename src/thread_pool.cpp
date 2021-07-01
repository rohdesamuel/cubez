#include "thread_pool.h"

// the constructor just launches some amount of workers
TaskThreadPool::TaskThreadPool(size_t threads, size_t max_queue_size)
  : stop_(false), max_queue_size_(max_queue_size) {

  for (size_t i = 0; i < max_queue_size_; ++i) {
    available_tasks_.push((qbHandle)i);
    tasks_.push_back({});
  }

  for (qbId task_id = 0; task_id < (qbId)threads; ++task_id) {
    workers_.emplace_back(
      [this, task_id] {
      for (;;) {
        Task* task;

        {
          std::unique_lock<std::mutex> lock(queue_mu_);
          task_available_.wait(lock, [this] { return stop_ || !tasks_queue_.empty(); });
          if (stop_)
            return;
          task = tasks_queue_.front();
          tasks_queue_.pop();
        }

        task->output.set_value(task->fn());
      }
    });
  }
}

// the destructor joins all threads
TaskThreadPool::~TaskThreadPool() {
  {
    std::unique_lock<std::mutex> lock(queue_mu_);
    stop_ = true;
  }
  task_available_.notify_all();
  for (std::thread &worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

qbTask TaskThreadPool::alloc_task() {
  qbHandle ret = qbInvalidHandle;
  while (!stop_) {
    available_tasks_mu_.lock();
    if (available_tasks_.empty()) {
      available_tasks_mu_.unlock();
      std::this_thread::yield();
      continue;
    }

    ret = available_tasks_.front();
    available_tasks_.pop();
    available_tasks_mu_.unlock();
    break;
  }
  return ret;
}

qbTask TaskThreadPool::enqueue(std::function<qbVar()>&& f) {
  qbTask ret = alloc_task();

  if (ret == qbInvalidHandle) {
    return ret;
  }

  {
    std::unique_lock<std::mutex> lock(queue_mu_);
    tasks_queue_.emplace(&tasks_[ret]);
    tasks_[ret].fn = f;
    tasks_[ret].output = std::promise<qbVar>();
  }
  task_available_.notify_one();

  return ret;
}

qbVar TaskThreadPool::join(qbTask task) {
  if (task >= max_queue_size_) {
    return qbNil;
  }

  qbVar ret = tasks_[task].output.get_future().get();
  {
    std::unique_lock<std::mutex> lock(available_tasks_mu_);
    available_tasks_.push(task);
  }
  return ret;
}