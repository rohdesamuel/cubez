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

#ifndef BARRIER__H
#define BARRIER__H

#include "defs.h"

#include <atomic>
#include <condition_variable>

class Barrier {
 public:
  class Ticket {
   public:
     Ticket(const Ticket&) = delete;
     Ticket(Ticket&&) = default;

     void lock();
     void unlock();

   private:
     Ticket(uint8_t order, int* serving, int* queue_size, std::mutex* mu,
            std::condition_variable* condition);

     const uint8_t order_;
     int* serving_;
     int* queue_size_;

     std::mutex* mu_;
     std::condition_variable* condition_;

     std::unique_lock<std::mutex> wait_lock_;
     friend class Barrier;
  };

  std::unique_ptr<Ticket> MakeTicket();

private:
  int serving_;
  int queue_size_;

  std::mutex mu_;
  std::condition_variable condition_;
};

#endif  // BARRIER__H