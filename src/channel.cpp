#include "channel.h"
#include "system_impl.h"

#include <cstring>

Channel::Channel(size_t size) : size_(size) {
  // Start count is arbitrarily choosen.
  for (int i = 0; i < 16; ++i) {
    free_mem_.enqueue(AllocMessage(nullptr));
  }
}

// Thread-safe.
void* Channel::AllocMessage(void* initial_val) {
  void* ret = nullptr;
  if (!free_mem_.try_dequeue(ret)) {
    ret = calloc(1, size_);
  }
  DEBUG_ASSERT(ret, QB_ERROR_NULL_POINTER);
  if (initial_val) {
    memmove(ret, initial_val, size_);
  }
  return ret;
}

qbResult Channel::SendMessage(void* message) {
  message_queue_.enqueue(AllocMessage(message));
  return qbResult::QB_OK;
}

qbResult Channel::SendMessageSync(void* message) {
  for (const auto& handler : handlers_) {
    (SystemImpl::FromRaw(handler))->Run(message);
  }

  FreeMessage(message);
  return qbResult::QB_OK;
}

void Channel::AddHandler(qbSystem s) {
  handlers_.insert(s);
}

void Channel::RemoveHandler(qbSystem s) {
  std::lock_guard<decltype(handlers_mu_)> lock(handlers_mu_);
  handlers_.erase(s);
}

void Channel::Flush() {
  std::lock_guard<decltype(handlers_mu_)> lock(handlers_mu_);
  void* msg = nullptr;
  while (message_queue_.try_dequeue(msg)) {
    for (const auto& handler : handlers_) {
      SystemImpl::FromRaw(handler)->Run(msg);
    }

    FreeMessage(msg);
  }
}

void Channel::FreeMessage(void* message) {
  free_mem_.enqueue(message);
}
