#ifndef CHANNEL__H
#define CHANNEL__H

#include "concurrentqueue.h"
#include "defs.h"

#include <mutex>
#include <queue>
#include <set>

class Channel {
 public:
  struct ChannelMessage {
    qbId handler;
    void* message;
  };
  Channel(qbId id, std::queue<ChannelMessage>* message_queue, size_t size = 1);

  // Thread-safe.
  qbResult SendMessage(void* message);

  // Thread-safe.
  qbResult SendMessageSync(void* message);

  // Not thread-safe.
  void AddHandler(qbSystem s);

  // Not thread-safe.
  void RemoveHandler(qbSystem s);

  // Not thread-safe.
  void Flush(void* message);

 private:
  // Thread-safe.
  void* AllocMessage(void* initial_val);

  // Thread-safe.
  void FreeMessage(void* message);

  std::set<qbSystem> handlers_;
  qbId id_;
  std::queue<Channel::ChannelMessage>* message_queue_;
  moodycamel::ConcurrentQueue<void*> free_mem_;
  size_t size_;
};

#endif  // CHANNEL__H

