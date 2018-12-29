#include "channel.h"
#include "system_impl.h"

#include <cstring>

Channel::Channel(
  qbId id,
  ByteQueue* message_queue,
  size_t size)
  : id_(id),
    message_queue_(message_queue),
    size_(size),
    mem_buffer_(size),
    free_mem_(size) {
  mem_buffer_.reserve(1000);
  free_mem_.reserve(1000);
}

Channel::ChannelMessage Channel::AllocMessage(void* initial_val) {
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

qbResult Channel::SendMessage(void* message) {
  Channel::ChannelMessage new_message = AllocMessage(message);
  message_queue_->push(&new_message);
  return qbResult::QB_OK;
}

qbResult Channel::SendMessageSync(void* message) {
  for (const auto& handler : handlers_) {
    (SystemImpl::FromRaw(handler))->Run(message);
  }

  return qbResult::QB_OK;
}

void Channel::AddHandler(qbSystem s) {
  handlers_.push_back(s);
}

void Channel::RemoveHandler(qbSystem s) {
  handlers_.erase(std::find(handlers_.begin(), handlers_.end(), s));
}

void Channel::Flush(size_t index) {
  for (const auto& handler : handlers_) {
    void* m = (uint8_t*)(ChannelMessage*)mem_buffer_[index] + size_;
    SystemImpl::FromRaw(handler)->Run(m);
  }
  FreeMessage(index);
}

void Channel::FreeMessage(size_t index) {
  free_mem_.push_back(index);
}
