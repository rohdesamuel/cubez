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

#include "barrier.h"

std::unique_ptr<Barrier::Ticket> Barrier::MakeTicket() {
  return std::unique_ptr<Barrier::Ticket>(
      new Barrier::Ticket(queue_size_++, &serving_, &queue_size_, &mu_,
                          &condition_));
}

Barrier::Ticket::Ticket(uint8_t order, int* serving, int* queue_size,
                        std::mutex* mu, std::condition_variable* condition)
    : order_(order),
      serving_(serving),
      queue_size_(queue_size),
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
