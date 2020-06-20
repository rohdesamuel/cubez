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

#include "program_thread.h"

#include "program_impl.h"

ProgramThread::ProgramThread(qbProgram* program, lua_State* lua_state) :
    program_(program), is_running_(false), lua_state_(lua_state) {}

ProgramThread::~ProgramThread() {
  Release();
}

void ProgramThread::Run(const std::function<GameState*()>& game_state_fn) {
  is_running_ = true;
  thread_.reset(new std::thread([this, game_state_fn]() {
    Coro main = coro_initialize(&main);
    while(is_running_) {
      ProgramImpl::FromRaw(program_)->Run(game_state_fn(), lua_state_);
    }
  }));
}

std::pair<qbProgram*, lua_State*> ProgramThread::Release() {
  if (is_running_) {
    is_running_ = false;
    thread_->join();
    thread_.reset();
  }
  return{ program_, lua_state_ };
}