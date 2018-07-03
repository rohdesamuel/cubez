#include "barrier.h"

std::unique_ptr<Barrier::Ticket> Barrier::MakeTicket() {
  return std::unique_ptr<Barrier::Ticket>(
      new Barrier::Ticket(queue_size_++, &serving_, &queue_size_, &mu_,
                          &condition_));
}

Barrier::Ticket::Ticket(uint8_t order, int* serving, int* queue_size,
                        std::mutex* mu, std::condition_variable* condition)
    : order_(order),
      queue_size_(queue_size),
      serving_(serving),
      mu_(mu), 
      condition_(condition) {
}

void Barrier::Ticket::lock() {
  wait_lock_ = std::unique_lock<std::mutex>(*mu_);
  condition_->wait(wait_lock_, [this]() { return *serving_ >= order_; });
}

void Barrier::Ticket::unlock() {
  ++(*serving_);
  if (*serving_ >= *queue_size_) {
    *serving_ = 0;
  }
  wait_lock_.unlock();
  condition_->notify_all();
}