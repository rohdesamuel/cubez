#ifndef EVENT_REGISTRY__H
#define EVENT_REGISTRY__H

#include "event.h"
#include "defs.h"
#include "byte_queue.h"
#include "game_state.h"

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

  void FlushAll(GameState* state);

 private:
  void AllocEvent(qbId id, qbEvent* event, Event* channel);

  // Requires state_mutex_.
  Event* FindEvent(qbEvent event);

  qbId program_;
  std::mutex state_mutex_;
  std::vector<Event*> events_;
  ByteQueue* message_queue_;
};

#endif  // EVENT_REGISTRY__H

