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

#include "event.h"
#include "system_impl.h"

#include <cstring>

Event::Event(qbId id, ByteQueue* message_queue, size_t size)
  : id_(id),
    message_queue_(message_queue),
    size_(size),
    mem_buffer_(size) {
  mem_buffer_.reserve(1000);
  free_mem_.reserve(1000);
}

Event::Message Event::AllocMessage(void* initial_val) {
  size_t ret_index = 0;
  if (free_mem_.empty()) {
    ret_index = mem_buffer_.size();
    mem_buffer_.push_back(initial_val);
    initial_val = nullptr;
  } else {
    ret_index = free_mem_.back();
    free_mem_.pop_back();
  }
  if (initial_val) {
    void* val = mem_buffer_[ret_index];
    memmove(val, initial_val, size_);
  }
  return{ id_, ret_index };
}

qbResult Event::SendMessage(void* message) {
  Event::Message new_message = AllocMessage(message);
  message_queue_->push(&new_message);
  return qbResult::QB_OK;
}

qbResult Event::SendMessageSync(void* message, GameState* state) {
  for (size_t i = 0; i < handlers_.size(); ++i) {
    handlers_[i](message, handler_args_[i]);
  }
  return qbResult::QB_OK;
}

void Event::AddHandler(qbEventFn fn, qbVar arg) {
  handlers_.push_back(fn);
  handler_args_.push_back(arg);
}

void Event::RemoveHandler(qbEventFn fn) {
  for (size_t i = 0; i < handlers_.size(); ++i) {
    if (handlers_[i] == fn) {
      handlers_.erase(handlers_.begin() + i);
      handler_args_.erase(handler_args_.begin() + i);
    }
  }
}

void Event::Flush(size_t index, GameState* state) {
  void* m = mem_buffer_[index];
  for (size_t i = 0; i < handlers_.size(); ++i) {
    handlers_[i](m, handler_args_[i]);
  }

  FreeMessage(index);
}

void Event::FreeMessage(size_t index) {
  free_mem_.push_back(index);
}
