#include <stdio.h>
#include <mutex>
#include <algorithm>

#include "internals.h"
#include "mem_pool.h"


typedef struct Block Block;


struct Block {
    Block *next;
};

struct FixedMemPool {
    size_t memb_size;
    size_t buff_size;
    Buffer *buff_head;
    Buffer *buff_last;
    Block *block_head;
    std::mutex mutex;
};

MemPoolError pool_fixed_init(FixedMemPool **pool, size_t block_size, size_t increase_count) {
  *pool = new FixedMemPool;
  if (!*pool) {
    return MEM_POOL_ERR_MALLOC;
  }
  (*pool)->memb_size = mem_align(std::max(block_size, sizeof(Block)));
  (*pool)->buff_size = increase_count * (*pool)->memb_size;
  (*pool)->buff_head = buffer_new((*pool)->buff_size);

  if (!(*pool)->buff_head) {
    return MEM_POOL_ERR_MALLOC;
  }

  (*pool)->buff_last = (*pool)->buff_head;
  (*pool)->block_head = NULL;


  return MEM_POOL_ERR_OK;
}

static MemPoolError from_free_list(FixedMemPool *pool, void **ptr) {
  Block *tmp = pool->block_head;
  pool->block_head = tmp->next;
  *ptr = tmp;
  pool->mutex.unlock();

  return MEM_POOL_ERR_OK;
}

static MemPoolError from_buffer(FixedMemPool *pool, Buffer *buff, void **ptr) {
  *ptr = buff->curr_ptr;
  buff->curr_ptr += pool->memb_size;
  pool->mutex.unlock();

  return MEM_POOL_ERR_OK;
}

MemPoolError pool_fixed_alloc(FixedMemPool *pool, void **ptr) {
  pool->mutex.lock();

  if (pool->block_head) {
    return from_free_list(pool, ptr);
  }
  Buffer *buff = pool->buff_last;

  if (buff->curr_ptr == buff->end) {
    buff = buffer_new(pool->buff_size);
    pool->buff_last->next = buff;
    pool->buff_last = buff;
  }

  return from_buffer(pool, buff, ptr);
}

MemPoolError pool_fixed_foreach(FixedMemPool *pool, FixedPoolForeach callback) {
  std::lock_guard<decltype(pool->mutex)> l(pool->mutex);

  Buffer *buff = pool->buff_head;

  while (buff) {
    for (char *block = buff->start; block < (char *)buff->curr_ptr; block += pool->memb_size) {
      if (MEM_POOL_FOREACH_STOP == callback(block)) {
        break;
      }
    }
    buff = buff->next;
  }

  return MEM_POOL_ERR_OK;
}

MemPoolError pool_fixed_free(FixedMemPool *pool, void *ptr) {
  std::lock_guard<decltype(pool->mutex)> l(pool->mutex);

  if (!BUFFER_LIST_HAS(pool->buff_head, ptr)) {
    return MEM_POOL_ERR_UNKNOWN_BLOCK;
  }

  Block *new_block = (Block *)ptr;
  Block *tmp = pool->block_head;
  pool->block_head = new_block;
  new_block->next = tmp;

  return MEM_POOL_ERR_OK;
}

MemPoolError pool_fixed_destroy(FixedMemPool *pool) {
  pool->mutex.unlock();
  delete pool;

  return MEM_POOL_ERR_OK;
}
