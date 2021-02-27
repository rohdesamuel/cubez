// Taken from https://github.com/progschj/ThreadPool
// This work has been modified from its original source.

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <cubez/async.h>

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
  auto enqueue(F&& f, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type>;
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
auto CoroThreadPool::enqueue(F&& f, Args&&... args)
  -> std::future<typename std::result_of<F(Args...)>::type>
{
  using return_type = typename std::result_of<F(Args...)>::type;

  auto task = std::make_shared< std::packaged_task<return_type()> >(
      std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
  std::future<return_type> res = task->get_future();
  {
    std::unique_lock<std::mutex> lock(queue_mutex);
    tasks.emplace([task](){ (*task)(); });
  }
  condition.notify_one();
  return res;
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
  TaskThreadPool(size_t threads);
  ~TaskThreadPool();

  template<class F>
  qbTask enqueue(F&& f);

  qbVar join(qbTask task);
  qbChannel input(qbId task_id, uint8_t input);

private:
  // need to keep track of threads so we can join them
  std::vector< std::thread > workers_;
  // the task queue
  std::queue< std::function<void(qbId)> > tasks_;
  std::vector<std::condition_variable*> tasks_cvs_;

  std::vector<std::vector<qbChannel>> task_inputs_;

  // synchronization
  std::mutex queue_mutex_;
  std::condition_variable condition_;
  bool stop_;
  size_t max_inputs_;
};

// add new work item to the pool
template<class F>
qbTask TaskThreadPool::enqueue(F&& f) {
  qbTask ret = new qbTask_{};
  ret->id_future = ret->id_promise.get_future();
  ret->output_future = ret->output_promise.get_future();

  {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    tasks_.emplace([this, f, ret](qbId task_id) {      
      ret->id_promise.set_value(task_id);
      ret->output_promise.set_value(f());
    });
  }
  condition_.notify_one();

  ret->task_id = ret->id_future.get();

  return ret;
}

#endif

