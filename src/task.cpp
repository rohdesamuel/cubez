/**
* Author: Samuel Rohde (rohde.samuel@cubez.io)
*
* Copyright 2020 Samuel Rohde
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

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