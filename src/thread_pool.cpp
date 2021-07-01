#include "thread_pool.h"

// the constructor just launches some amount of workers
TaskThreadPool::TaskThreadPool(size_t threads, size_t max_queue_size)
  : stop_(false), max_queue_size_(max_queue_size) {

  for (size_t i = 0; i < max_queue_size_; ++i) {
    available_tasks_.push((qbHandle)i);
    tasks_.push_back({});
  }

  for (size_t i = 0; i < threads; ++i) {
    workers_.emplace_back(
      [this] {
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
  if (ret == qbInvalidHandle) {
    return ret;
  }

  ret |= (uint64_t)tasks_[ret].generation << 32;
  return ret;
}

void TaskThreadPool::return_task(qbTask task) {
  uint32_t id = Task::get_task_id(task);
  std::unique_lock<std::mutex> lock(available_tasks_mu_);
  available_tasks_.push(id);
  ++tasks_[id].generation;
  if (tasks_[id].generation == qbInvalidHandle) {
    tasks_[id].generation = 0;
  }
}

qbTask TaskThreadPool::enqueue(std::function<qbVar()>&& f) {
  const qbTask ret = alloc_task();

  if (ret == qbInvalidHandle) {
    return ret;
  }

  uint32_t id = Task::get_task_id(ret);

  {
    std::unique_lock<std::mutex> lock(queue_mu_);
    tasks_queue_.emplace(&tasks_[id]);
    tasks_[id].fn = f;
    tasks_[id].output = std::promise<qbVar>();
  }
  task_available_.notify_one();

  return ret;
}

qbTask TaskThreadPool::dispatch(std::function<qbVar(qbTask)>&& f) {
  const qbTask task = alloc_task();

  if (task == qbInvalidHandle) {
    return task;
  }

  uint32_t id = Task::get_task_id(task);
  {
    std::unique_lock<std::mutex> lock(queue_mu_);
    tasks_queue_.emplace(&tasks_[id]);
    tasks_[id].fn = [f, task, this]() {
      qbVar ret = f(task);
      return_task(task);
      return ret;
    };
    tasks_[id].output = std::promise<qbVar>();
  }
  task_available_.notify_one();
  return task;
}

qbVar TaskThreadPool::join(qbTask task) {
  uint32_t id = Task::get_task_id(task);

  if (id >= max_queue_size_) {
    return qbNil;
  }

  qbVar ret = tasks_[id].output.get_future().get();
  return_task(task);
  return ret;
}

qbBool TaskThreadPool::is_active(qbTask task) {
  uint32_t id = Task::get_task_id(task);
  uint32_t generation = Task::get_generation(task);

  if (task == qbInvalidHandle || generation == qbInvalidHandle) {
    return QB_FALSE;
  }

  return tasks_[id].generation == generation;
}