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

#ifndef BUDDY_SYSTEM_ALLOCATOR__H
#define BUDDY_SYSTEM_ALLOCATOR__H

#include "common.h"
#include "fast_math.h"

template<class Ty_>
class BuddySystemAllocator {
private:
  struct Block {
    uint64_t order;
    // Null implies that the child is free memory.
    struct Block* next = nullptr;
  };

  uint8_t* mem_;
  Block** free_blocks_;

  uint64_t capacity_;
  uint64_t tot_size_;
  uint64_t order_;
  uint64_t size_;

  inline bool in_range(Ty_* mem) {
    return (uint8_t*)mem >= mem_ && (uint8_t*)mem < mem_ + tot_size_;
  }

  inline Ty_* block_to_ptr(Block* b) {
    return (Ty_*)((uint8_t*)b + sizeof(Block));
  }

  inline Block* ptr_to_block(Ty_* mem) {
    return (Block*)((uint8_t*)mem - sizeof(Block));
  }

  Block* merge(Block* head, Block* tail, Block** other) {
    if (head < tail && tail == buddy_of(head)) {
      ++head->order;
      head->next = nullptr;
      *other = tail;
      return head;
    } else if (head > tail && head == buddy_of(tail)) {
      ++tail->order;
      tail->next = nullptr;
      *other = head;
      return tail;
    }
    return nullptr;
  }

  inline Block* buddy_of(Block* block) {
    return (Block*)((uint8_t*)block + size_ * ((uint64_t)1 << block->order));
  }

  void release(Block* block) {
    uint64_t order = block->order;

    Block* node = free_blocks_[order];
    Block* prev = node;
    if (!node->next) {
      node->next = block;
    } else {
      while (node->next) {
        prev = node;
        node = node->next;

        Block* other = nullptr;
        Block* merged = merge(block, node, &other);
        if (merged) {
          prev->next = prev->next->next;
          other->order = 0;
          other->next = nullptr;
          release(merged);
          return;
        }
      }
    }
  }

  Block* split(Block* head) {
    --head->order;
    Block* tail = buddy_of(head);
    tail->order = head->order;
    return tail;
  }

  Block* alloc_block(uint64_t order) {
    if (order <= order_) {
      // Get the block of memory to allocate from
      Block* sentinel = free_blocks_[order];
      Block* mem = sentinel->next;

      // If there are no free blocks, we need to allocate some.
      if (mem) {
        // Otherwise, we can get some memory right away!
        sentinel->next = mem->next;
        return mem;
      } else {
        // Recurse up to split the block.
        Block* head = alloc_block(order + 1);

        // If we get a nullptr, that means we have run out of memory.
        if (head) {
          // Split the block and push it onto the working memory.
          Block* tail = split(head);
          head->next = sentinel->next;
          sentinel->next = head;
          return tail;
        }
      }
    }
    return nullptr;
  }

public:
  BuddySystemAllocator(const uint64_t count) {
    size_ = sizeof(Block) + sizeof(Ty_);
    order_ = log_2(count) + (count_bits(count) == 1 ? 0 : 1);
    capacity_ = (uint64_t)1 << order_;
    tot_size_ = size_ * capacity_;
    mem_ = (uint8_t*)std::calloc(1, tot_size_);
    ((Block*)(mem_))->order = order_;
    ((Block*)(mem_))->next = nullptr;

    free_blocks_ = (Block**)std::calloc(order_ + 1, sizeof(Block*));
    for (uint64_t i = 0; i < order_ + 1; ++i) {
      free_blocks_[i] = (Block*)std::calloc(1, sizeof(Block));
      free_blocks_[i]->order = 0;
      free_blocks_[i]->next = nullptr;
    }
    free_blocks_[order_]->next = (Block*)mem_;
  }

  ~BuddySystemAllocator() {
    for (uint64_t i = 0; i < order_ + 1; ++i) {
      std::free(free_blocks_[i]);
    }
    std::free(free_blocks_);
    std::free(mem_);
  }

  Ty_* alloc(uint64_t count) {
    if (count == 0 || count > capacity_) {
      return nullptr;
    }
    uint64_t order = log_2(count) + (count_bits(count) == 1 ? 0 : 1);
    Block* b = alloc_block(order);
    return b ? block_to_ptr(b) : nullptr;
  }

  void release(Ty_* mem) {
    if (in_range(mem)) {
      Block* block = ptr_to_block(mem);
      release(block);
    }
  }

  void clear() {
    for (uint64_t i = 0; i < order_ + 1; ++i) {
      free_blocks_[i]->next = nullptr;
    }
    free_blocks_[order_]->next = (Block*)mem_;
    ((Block*)mem_)->order = order_;
    ((Block*)mem_)->next = nullptr;
  }

  uint64_t capacity() {
    return capacity_;
  }
};

#endif /*BUDDY_SYSTEM_ALLOCATOR__H*/