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
  // Called once per fixed update (once per frame) that implementations can
  // use to do janitorial work.
  void (*flush)(qbMemoryAllocator_* self, int64_t frame);

  // Optional.
  // Invalidates all currently allocated memory without deleting the buffer.
  void (*clear)(qbMemoryAllocator_* self);
} *qbMemoryAllocator, qbMemoryAllocator_;

#define qb_alloc(allocator, size) (allocator)->alloc((allocator), (size))
#define qb_dealloc(allocator, ptr) (allocator)->dealloc((allocator), (ptr))

QB_API qbResult qb_memallocator_register(const char* name, qbMemoryAllocator allocator);
QB_API qbMemoryAllocator qb_memallocator_find(const char* name);
QB_API qbResult qb_memallocator_unregister(const char* name);

// Returns the default memory allocator using malloc/free.
// This is registered with the name "default".
QB_API qbMemoryAllocator qb_memallocator_default();

// Creates a thread-safe memory pool allocator.
// 
// Description: A memory pool allocator keeps track of slabs of memory in a
//              linked list. Every allocation searches in the list of allocated
//              blocks and finds the slab with the best fit. If not, allocates
//              a new slab.
// 
// Behavior:
//   - alloc(size): Allocates a block of memory of size `size`. Returns nullptr
//                  if host is OOM.
//   - dealloc(ptr): deallocates the given pointer.
//   - flush(frame): noop
//   - clear(): noop
QB_API qbMemoryAllocator qb_memallocator_pool();

// Creates a thread-safe linear allocator.
// 
// Description: A linear allocator is a simple stack allocator. The memory is
//              pre-allocated and an offset is moved based on how much memory
//              is `alloc`ed. Memory cannot be deallocated. 
// 
// Behavior:
//   - alloc(size): Allocates a block of memory of size `size`. Returns nullptr
//                  if size + total allocated is greater than max size.
//   - dealloc(ptr): noop
//   - flush(frame): noop
//   - clear(): resets offset to zero.
QB_API qbMemoryAllocator qb_memallocator_linear(size_t max_size);

// Creates a thread-safe paged allocator.
// 
// Description: A paged allocator is a linear allocator without a max size. The
//              memory is allocated in `page_size` increments, OR `size` if the
//              size is greater than the page size.
// 
// Behavior:
//   - alloc(size): Allocates a block of memory of size `size`.
//   - dealloc(ptr): noop
//   - flush(frame): noop
//   - clear(): Deallocates all memory.
QB_API qbMemoryAllocator qb_memallocator_paged(size_t page_size);

// Creates a thread-safe stack allocator.
// 
// Description: A stack allocator acts like a linear allocator that allows
//              deallocation. This is implemented by tracking the size of each
//              allocation in a header. Memory needs to be popped off in order
//              of allocation.
// 
// Behavior:
//   - alloc(size): Allocates a block of memory of size `size` on top of the
//                  stack. Returns nullptr if size + total allocated is
//                  greater than max size.
//   - dealloc(ptr): Deallocates the given pointer. Assumes that the pointer
//                   is at the top of the stack. The stack becomes corrupted if
//                   trying to deallocate a pointer not at the top.
//   - flush(frame): noop
//   - clear(): resets offset to zero.
QB_API qbMemoryAllocator qb_memallocator_stack(size_t max_size);

// Creates a thread-safe object pool allocator.
// 
// Description: An object pool allocator holds a preallocated number of
//              objects. Allocating from the object pool retrieves an object
//              from the pool. The pool has a `pool_count` objects of size
//              `object_size`.
// 
// Behavior:
//   - alloc(unused): Returns an object if available. Returns nullptr if the
//                    pool has allocated all objects already.
//   - dealloc(ptr): Returns the object to the pool.
//   - flush(frame): noop
//   - clear(): Returns all allocated objects to the pool. Any reference to an
//              allocated object becomes invalid.
QB_API qbMemoryAllocator qb_memallocator_objectpool(size_t object_size, size_t pool_count);

#endif  // CUBEZ_MEMORY__H