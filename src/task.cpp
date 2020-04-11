#include "task.h"
#include "program_impl.h"

#include "coro.h"

Task::Task(qbProgram* program) : task_(program) {
  stop_ = false;
  game_state_ = nullptr;

  thread_ = new std::thread([this]() {
    Coro main = coro_initialize(&main);
    for (;;) {
      std::unique_lock<std::mutex> lock(state_lock_);
      state = WAITING;
      should_run_.wait(lock, [this] { return game_state_ != nullptr || stop_; });
      
      if (stop_) {
        state = STOPPED;
        break;
      }

      ProgramImpl::FromRaw(task_)->Run(game_state_);
      game_state_ = nullptr;
      should_wait_.notify_all();
    }
  });
  thread_->detach();
}

Task::~Task() {
  Stop();
  Wait();
}

void Task::Ready() {
  ProgramImpl::FromRaw(task_)->Ready();
}

void Task::Run(GameState* game_state) {
  std::unique_lock<std::mutex> lock(state_lock_);
  game_state_ = game_state;
  should_run_.notify_one();
}

void Task::Done() {
  ProgramImpl::FromRaw(task_)->Done();
}

void Task::Stop() {
  std::unique_lock<std::mutex> lock(state_lock_);
  stop_ = true;
  should_run_.notify_one();
}

void Task::Wait() {
  std::unique_lock<std::mutex> lock(state_lock_);
  should_wait_.wait(lock, [this] { return game_state_ == nullptr; });
}