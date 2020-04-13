/**
* Author: Samuel Rohde (rohde.samuel@cubez.io)
*
* Copyright {2020} {Samuel Rohde}
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

#ifndef TASK__H
#define TASK__H

#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include "game_state.h"

class Task {
public:
  Task(qbProgram* program);
  ~Task();

  void Ready();

  void Run(GameState* game_state);

  void Done();

  void Stop();

  void Wait();

private:
  typedef enum {
    STOPPED,
    WAITING,
  } State;

  std::mutex state_lock_;
  std::condition_variable should_run_;
  std::condition_variable should_wait_;

  State state;
  std::atomic_bool stop_;

  std::thread* thread_;
  GameState* game_state_;
  qbProgram* task_;
};

#endif  // TASK__H