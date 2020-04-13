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

#ifndef PROGRAM_REGISTRY__H
#define PROGRAM_REGISTRY__H

#include "defs.h"
#include "program_impl.h"
#include "program_thread.h"
#include "thread_pool.h"
#include "task.h"

#include <algorithm>
#include <future>
#include <map>

class ProgramRegistry {
 public:
  ProgramRegistry();

  qbId CreateProgram(const char* program);

  qbResult DetatchProgram(qbId program, const std::function<GameState*()>& game_state_fn);

  qbResult JoinProgram(qbId program);

  qbProgram* GetProgram(qbId id);

  void Run(GameState* state);

  qbResult RunProgram(qbId program, GameState* state);

 private:
  void RunMain(GameState* state);

  qbProgram* AllocProgram(qbId id, const char* name);

  std::atomic_long id_;
  qbProgram* main_program_;
  SparseMap<qbProgram*, std::vector<qbProgram*>> programs_;
  std::unordered_map<size_t, std::unique_ptr<ProgramThread>> detached_;
  std::unordered_map<size_t, Task*> program_threads_;
};

#endif  // PROGRAM_REGISTRY__H

