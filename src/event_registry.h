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

