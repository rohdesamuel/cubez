#include "internals.h"
#include "mem_pool.h"

#include <algorithm>
#include <mutex>


struct SizedBlock {
    Header header;
    SizedBlock *next_in_free_list;
};

struct VariableMemPool {
    size_t buff_size;
    size_t header_size;
    uint16_t tolerance_percent;
    Buffer *buff_head;
    Buffer *buff_last;
    SizedBlock *block_head;
    std::mutex mutex;
};


MemPoolError pool_variable_init(VariableMemPool **pool, size_t grow_size, uint16_t tolerance_percent) {
  *pool = new VariableMemPool;
  if (!*pool) {
    return MEM_POOL_ERR_MALLOC;
  }
  /* (*pool)->tolerance_percent = tolerance_percent <= MEM_POOL_NO_BEST_FIT ? MEM_POOL_NO_BEST_FIT : tolerance_percent; */
  (*pool)->tolerance_percent = tolerance_percent >= MEM_POOL_NO_BEST_FIT ? MEM_POOL_NO_BEST_FIT : tolerance_percent;
  (*pool)->header_size = mem_align(sizeof(Header));
  (*pool)->buff_size = grow_size;
  (*pool)->buff_head = buffer_new(grow_size);

  if (!(*pool)->buff_head) {
    return MEM_POOL_ERR_MALLOC;
  }

  (*pool)->buff_last = (*pool)->buff_head;
  (*pool)->block_head = NULL;

  return MEM_POOL_ERR_OK;
}

static void *from_buffer(Buffer *buff, size_t header_size, size_t block_size) {
  Header *header = (Header*)buff->curr_ptr;
  header->size = block_size;
  header->prev_in_buff = (SizedBlock*)buff->prev_ptr;

  buff->prev_ptr = buff->curr_ptr;
  buff->curr_ptr += (header_size + block_size);

  return (char *)header + header_size;
}

static void *best_fit_from_free_list(VariableMemPool *pool, size_t required_size) {
  SizedBlock **curr = &pool->block_head;
  size_t block_size;
  long diff;
  size_t diff_percent;

  while (*curr) {
    block_size = (*curr)->header.size;
    diff = labs((long)block_size - (long)required_size);
    diff_percent = ((size_t)diff * 100) / ((block_size + required_size) / 2);

    if (MEM_POOL_NO_BEST_FIT == pool->tolerance_percent || diff_percent <= pool->tolerance_percent) {
      SizedBlock *block = *curr;
      *curr = (*curr)->next_in_free_list;

      return (char *)block + pool->header_size;
    }
    curr = &(*curr)->next_in_free_list;
  }

  return NULL;
}

MemPoolError pool_variable_alloc(VariableMemPool *pool, size_t size, void **ptr) {
  std::lock_guard<decltype(pool->mutex)> l(pool->mutex);

  Buffer *buff = pool->buff_last;
  size_t block_size = mem_align(size);

  if (pool->block_head && (*ptr = best_fit_from_free_list(pool, block_size))) {
    return MEM_POOL_ERR_OK;
  }

  if (!buffer_has_space(buff, pool->header_size + block_size)) {
    buff->next = buffer_new(pool->header_size + std::max(pool->buff_size, block_size));
    buff = buff->next;
    pool->buff_last = buff;
  }
  *ptr = from_buffer(buff, pool->header_size, block_size);

  return MEM_POOL_ERR_OK;
}

static int delete_block_from_free_list(VariableMemPool *pool, SizedBlock *block) {
  SizedBlock **curr = &pool->block_head;

  while (*curr) {
    if ((*curr) == block) {
      *curr = block->next_in_free_list;
      return 1;
    }
    curr = &(*curr)->next_in_free_list;
  }

  return 0;
}

static SizedBlock *append(SizedBlock *to, SizedBlock *from, size_t header_size) {
  to->header.size += from->header.size + header_size;

  return to;
}

static SizedBlock *merge_next_free_blocks(VariableMemPool *pool, Buffer *buff, SizedBlock *block) {
  void *next = NULL;

  while (1) {
    next = (SizedBlock *)((char *)block + block->header.size + pool->header_size);

    if (buffer_has(buff, (char*)next) && delete_block_from_free_list(pool, (SizedBlock*)next)) {
      block = append(block, (SizedBlock*)next, pool->header_size);
    } else {
      break;
    }
  }

  return block;
}

static SizedBlock *merge_previous_free_blocks(VariableMemPool *pool, SizedBlock *block) {
  SizedBlock *prev = block->header.prev_in_buff;

  while (prev) {
    if (!delete_block_from_free_list(pool, prev)) {
      break;
    }
    block = append(prev, block, pool->header_size);
    prev = prev->header.prev_in_buff;
  }

  return block;
}

static SizedBlock *defragment(VariableMemPool *pool, Buffer *buff, SizedBlock *block) {
  block = merge_next_free_blocks(pool, buff, block);
  block = merge_previous_free_blocks(pool, block);

  return block;
}

MemPoolError pool_variable_free(VariableMemPool *pool, void *ptr) {
  std::lock_guard<decltype(pool->mutex)> l(pool->mutex);

  Buffer *buff = buffer_list_find(pool->buff_head, ptr);
  SizedBlock *new_block = (SizedBlock *)((char *)ptr - pool->header_size);

  if (!buff) {
    return MEM_POOL_ERR_UNKNOWN_BLOCK;
  } else {
    new_block = defragment(pool, buff, new_block);
  }
  SizedBlock *tmp = pool->block_head;
  pool->block_head = new_block;
  new_block->next_in_free_list = tmp;

  return MEM_POOL_ERR_OK;
}

MemPoolError pool_variable_aligned_sizeof(VariableMemPool *pool, void *ptr, size_t *size) {
  std::lock_guard<decltype(pool->mutex)> l(pool->mutex);

  if (!buffer_list_find(pool->buff_head, ptr)) {
    return MEM_POOL_ERR_UNKNOWN_BLOCK;
  }

  SizedBlock *block = (SizedBlock *)((char *)ptr - pool->header_size);
  *size = block->header.size;;

  return MEM_POOL_ERR_OK;
}

MemPoolError pool_variable_destroy(VariableMemPool *pool) {
  pool->mutex.unlock();
  delete pool;

  return MEM_POOL_ERR_OK;
}
