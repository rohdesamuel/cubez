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