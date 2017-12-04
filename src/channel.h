#ifndef CHANNEL__H
#define CHANNEL__H

#include "concurrentqueue.h"
#include "defs.h"

#include <mutex>
#include <set>

class Channel {
 public:
  Channel(size_t size = 1);

  qbResult SendMessage(void* message);

  qbResult SendMessageSync(void* message);

  void AddHandler(qbSystem s);

  void RemoveHandler(qbSystem s);

  void Flush();

 private:
  // Thread-safe.
  void* AllocMessage(void* initial_val);

  // Thread-safe.
  void FreeMessage(void* message);

  std::set<qbSystem> handlers_;
  moodycamel::ConcurrentQueue<void*> message_queue_;
  moodycamel::ConcurrentQueue<void*> free_mem_;
  size_t size_;
};

#endif  // CHANNEL__H

