#ifndef MEM_POOL_INTERNALS_H
#define MEM_POOL_INTERNALS_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

#include "mem_pool.h"

#define MEMPOOL_IS_ASSOCIATED(pool, ptr, err)         \
    lock(pool);                                     \
    if (buffer_list_has(pool->buff_head, ptr)) {    \
        err = MEM_POOL_ERR_OK;                      \
    } else {                                        \
        err = MEM_POOL_ERR_UNKNOWN_BLOCK;           \
    }                                               \
    unlock(pool)                                    \

#define BUFFER_LIST_HAS(head, ptr) (NULL != buffer_list_find(head, ptr))


typedef struct Buffer Buffer;

struct Buffer {
    char *start;
    char *prev_ptr;
    char *curr_ptr; /* This is only tracked for variadic blocks */
    char *end;
    Buffer *next;
};

typedef struct Header Header;

typedef struct SizedBlock SizedBlock;

struct Header {
    size_t size;
    SizedBlock *prev_in_buff;
};

Buffer *buffer_new(size_t size);

void buffer_list_destroy(Buffer *head);

Buffer *buffer_list_find(Buffer *head, void *ptr);

static inline bool buffer_has_space(Buffer *buff, size_t size)
{
    return (char *)buff->end - (char *)buff->curr_ptr >= (long)size;
}

static inline bool buffer_has(Buffer *buff, char *ptr)
{
    return ptr >= buff->start && ptr <= buff->end;
}


#endif
