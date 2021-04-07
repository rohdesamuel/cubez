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

#ifndef CUBEZ_MEMORY__H
#define CUBEZ_MEMORY__H

#include <cubez/cubez.h>

typedef struct qbMemoryAllocator_ {
  // Destroys this memory allocator. All memory currently allocated is invalid.
  void (*destroy)(qbMemoryAllocator_* self);

  // Allocate a block of memory with `size`.
  void* (*alloc)(qbMemoryAllocator_* self, size_t size);

  // Deallocate a block of memory.
  void (*dealloc)(qbMemoryAllocator_* self, void* ptr);

  // Optional.
  // Called once per fixed update  (once per frame) that implementations can
  // use to do janitorial work.
  void (*flush)(qbMemoryAllocator_* self, int64_t frame);
} *qbMemoryAllocator, qbMemoryAllocator_;

#define qb_alloc(allocator, size) (allocator)->alloc((allocator), (size))
#define qb_dealloc(allocator, ptr) (allocator)->dealloc((allocator), (ptr))

QB_API qbResult qb_memallocator_register(const char* name, qbMemoryAllocator allocator);
QB_API qbMemoryAllocator qb_memallocator_find(const char* name);
QB_API qbResult qb_memallocator_unregister(const char* name);

// Returns the default memory allocator using malloc/free/realloc.
// This is registered with the name "default".
QB_API qbMemoryAllocator qb_memallocator_default();

// Creates a thread-safe memory pool allocator.
QB_API qbMemoryAllocator qb_memallocator_pool();

#endif  // CUBEZ_MEMORY__H