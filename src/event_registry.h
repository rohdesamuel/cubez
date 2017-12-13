#ifndef EVENT_REGISTRY__H
#define EVENT_REGISTRY__H

#include "channel.h"
#include "defs.h"

#include <mutex>
#include <unordered_map>

class EventRegistry {
 public:
  EventRegistry(qbId program);
  ~EventRegistry();

  // Thread-safe.
  qbResult CreateEvent(qbEvent* event, qbEventAttr attr);

  // Thread-safe.
  void Subscribe(qbEvent event, qbSystem system);

  // Thread-safe.
  void Unsubscribe(qbEvent event, qbSystem system);

  void FlushAll();

 private:
  void AllocEvent(qbId id, qbEvent* event, Channel* channel);

  // Requires state_mutex_.
  Channel* FindEvent(qbEvent event);

  qbId program_;
  qbId event_id_;
  std::mutex state_mutex_;
  std::unordered_map<qbId, Channel*> events_;
  std::queue<Channel::ChannelMessage>* message_queue_;
};

#endif  // EVENT_REGISTRY__H

