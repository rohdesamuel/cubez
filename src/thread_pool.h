// Taken from https://github.com/progschj/ThreadPool
// This work has been modified from its original source.

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <cubez/async.h>

#include <type_traits>
#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include "coro.h"
#include "defs.h"

class CoroThreadPool {
public:
  CoroThreadPool(size_t);
  template<class F, class... Args>
  void enqueue(F&& f, Args&&... args);
  ~CoroThreadPool();
private:
  // need to keep track of threads so we can join them
  std::vector< std::thread > workers;
  // the task queue
  std::queue< std::function<void()> > tasks;
  
  // synchronization
  std::mutex queue_mutex;
  std::condition_variable condition;
  bool stop;
};
 
// the constructor just launches some amount of workers
inline CoroThreadPool::CoroThreadPool(size_t threads)
  :   stop(false) {
  for (size_t i = 0; i < threads; ++i) {
    workers.emplace_back(
      [this] {
      Coro main = coro_initialize(&main);
      for (;;) {
        std::function<void()> task;

        {
          std::unique_lock<std::mutex> lock(this->queue_mutex);
          this->condition.wait(lock,
                               [this] { return this->stop || !this->tasks.empty(); });
          if (this->stop && this->tasks.empty())
            return;
          task = std::move(this->tasks.front());
          this->tasks.pop();
        }

        task();
      }
    });
  }
}

// add new work item to the pool
template<class F, class... Args>
void CoroThreadPool::enqueue(F&& f, Args&&... args)
{
  auto task = std::make_shared< std::packaged_task<void()> >(
      std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
  {
    std::unique_lock<std::mutex> lock(queue_mutex);
    tasks.emplace([task](){ (*task)(); });
  }
  condition.notify_one();
}

// the destructor joins all threads
inline CoroThreadPool::~CoroThreadPool()
{
  {
    std::unique_lock<std::mutex> lock(queue_mutex);
    stop = true;
  }
  condition.notify_all();
  for (std::thread &worker : workers) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

class TaskThreadPool {
public:  
  TaskThreadPool(size_t threads, size_t max_queue_size);
  ~TaskThreadPool();

  qbTask enqueue(std::function<qbVar()>&& f);
  qbTask dispatch(std::function<qbVar(qbTask)>&& f);
  qbVar join(qbTask task);
  qbBool is_active(qbTask task);
private:
  struct Task {
    std::function<qbVar()> fn;
    std::promise<qbVar> output;
    uint32_t generation;

    static uint32_t get_generation(qbTask task) {
      return task >> 32;
    }

    static uint32_t get_task_id(qbTask task) {
      return (uint64_t)task & 0x00000000FFFFFFFF;
    }
  };

  qbTask alloc_task();
  void return_task(qbTask task);

  // need to keep track of threads so we can join them
  std::vector< std::thread > workers_;

  std::mutex available_tasks_mu_;
  std::queue<qbHandle> available_tasks_;

  std::vector<Task> tasks_;

  // synchronization
  std::mutex queue_mu_;
  std::queue<Task*> tasks_queue_;
  std::condition_variable task_available_;
  bool stop_;
  
  const size_t max_queue_size_;
};

#endif