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

#ifndef PROGRAM_THREAD__H
#define PROGRAM_THREAD__H

#include "defs.h"

#include <atomic>
#include <memory>
#include <thread>


class ProgramThread {
 public:
  ProgramThread(qbProgram* program);

  ~ProgramThread();

  void Run(const std::function<GameState*()>& game_state_fn);
  
  qbProgram* Release();

 private:
  qbProgram* program_;
  std::unique_ptr<std::thread> thread_;
  std::atomic_bool is_running_;
};

#endif  // PROGRAM_THREAD__H

