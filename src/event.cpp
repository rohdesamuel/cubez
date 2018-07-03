#include "event.h"
#include "system_impl.h"

#include <cstring>

Event::Event(qbId program, qbId id, ByteQueue* message_queue,
             size_t size)
  : program_(program),
    id_(id),
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
  for (const auto& handler : handlers_) {
    (SystemImpl::FromRaw(handler))->Run(state, message);
  }

  return qbResult::QB_OK;
}

void Event::AddHandler(qbSystem s) {
  handlers_.push_back(s);
}

void Event::RemoveHandler(qbSystem s) {
  handlers_.erase(std::find(handlers_.begin(), handlers_.end(), s));
}

void Event::Flush(size_t index, GameState* state) {
  for (const auto& handler : handlers_) {
    void* m = mem_buffer_[index];
    SystemImpl::FromRaw(handler)->Run(state, m);
  }
  FreeMessage(index);
}

void Event::FreeMessage(size_t index) {
  free_mem_.push_back(index);
}
