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

#include "program_registry.h"
#include "private_universe.h"

#include <cstring>

ProgramRegistry::ProgramRegistry() :
    program_threads_(std::thread::hardware_concurrency()) {
  id_ = 0;
}

qbId ProgramRegistry::CreateProgram(const char* program) {
  qbId id = id_++;

  qbProgram* p = AllocProgram(id, program);
  programs_[id] = p;

  if (id > 0) {
    program_threads_[id] = new Task(p);
  } else {
    main_program_ = p;
  }
  return id;
}

qbResult ProgramRegistry::DetatchProgram(qbId program, const std::function<GameState*()>& game_state_fn) {
  if (programs_.has(program)) {
    qbProgram* to_detach = GetProgram(program);
    std::unique_ptr<ProgramThread> program_thread(
      new ProgramThread(to_detach));
    program_thread->Run(game_state_fn);
    detached_[program] = std::move(program_thread);
    programs_.erase(program);
    program_threads_.erase(program);
  }
  return QB_OK;
}

qbResult ProgramRegistry::JoinProgram(qbId program) {  
  qbProgram* prog = programs_[program] = detached_.find(program)->second->Release();
  program_threads_[program] = new Task(prog);
  detached_.erase(detached_.find(program));
  return QB_OK;
}

qbProgram* ProgramRegistry::GetProgram(qbId id) {
  if (!programs_.has(id)) {
    return nullptr;
  }
  return programs_[id];
}

void ProgramRegistry::Run(GameState* state) {
  for (auto& task : program_threads_) {
    task.second->Ready();
  }

  for (auto& task : program_threads_) {
    task.second->Run(state);
  }

  RunProgram(main_program_->id, state);

  for (auto& task : program_threads_) {
    task.second->Wait();
  }

  for (auto& task : program_threads_) {
    task.second->Done();
  }
}

qbResult ProgramRegistry::RunProgram(qbId program, GameState* state) {
  ProgramImpl* p = (ProgramImpl*)programs_[program]->self;
  p->Run(state);
  return QB_OK;
}

qbProgram* ProgramRegistry::AllocProgram(qbId id, const char* name) {
  qbProgram* p = (qbProgram*)calloc(1, sizeof(qbProgram));
  *(qbId*)(&p->id) = id;
  *(char**)(&p->name) = STRDUP(name);
  p->self = new ProgramImpl(p);

  return p;
}
