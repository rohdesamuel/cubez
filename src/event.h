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

#ifndef EVENT__H
#define EVENT__H

#include "concurrentqueue.h"
#include "defs.h"
#include "byte_vector.h"
#include "byte_queue.h"
#include "memory_pool.h"
#include "game_state.h"

#include <mutex>
#include <queue>
#include <set>

class Event {
 public:
  struct Message {
    qbId handler;
    size_t index;
  };

  Event(qbId program, qbId id, ByteQueue* message_queue, size_t size = 1);

  // Thread-safe.
  qbResult SendMessage(void* message);

  // Thread-safe.
  qbResult SendMessageSync(void* message, GameState* state);

  // Not thread-safe.
  void AddHandler(qbSystem s);

  // Not thread-safe.
  void RemoveHandler(qbSystem s);

  // Not thread-safe.
  void Flush(size_t index, GameState* state);

 private:
  // Allocates a message to send. Moves the data pointed to by initial_val
  // to a new message. Returns pointer to the newly allocated message.
   Message AllocMessage(void* initial_val);

  // Thread-safe.
  void FreeMessage(size_t index);

  std::vector<qbSystem> handlers_;
  qbId program_;
  qbId id_;
  ByteQueue* message_queue_;
  size_t size_;
  ByteVector mem_buffer_;
  std::vector<size_t> free_mem_;
};

#endif  // EVENT__H
