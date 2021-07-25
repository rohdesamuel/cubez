#include <cubez/memory.h>

#include "mem-pool/mem_pool.h"

#include <iostream>
#include <unordered_map>
#include <vector>

std::unordered_map<std::string, qbMemoryAllocator> registered;
std::vector<qbMemoryAllocator> flushables;

qbMemoryAllocator default_alloc;

struct MallocAllocator {
  qbMemoryAllocator_ self;
};

qbResult qb_memallocator_register(const char* name, qbMemoryAllocator allocator) {
  if (registered.count(name) > 0) {
    return QB_ERROR_ALREADY_EXISTS;
  }

  registered[name] = allocator;
  if (allocator->flush) {
    flushables.push_back(allocator);
  }

  return QB_OK;
}

qbMemoryAllocator qb_memallocator_find(const char* name) {
  if (registered.count(name) > 0) {
    return nullptr;
  }

  return registered[name];
}

qbResult qb_memallocator_unregister(const char* name) {
  registered.erase(name);
  return QB_OK;
}

void memory_initialize() {
  {
    MallocAllocator* alloc = new MallocAllocator;
    alloc->self.destroy = [](qbMemoryAllocator_*) {};
    alloc->self.alloc = [](qbMemoryAllocator_*, size_t size) { return malloc(size); };
    alloc->self.dealloc = [](qbMemoryAllocator_*, void* ptr) { free(ptr); };
    alloc->self.flush = nullptr;
    alloc->self.clear = nullptr;

    qb_memallocator_register("default", (qbMemoryAllocator_*)alloc);
    default_alloc = (qbMemoryAllocator_*)alloc;
  }
}

void memory_flush_all(int64_t frame) {
  for (auto m : flushables) {
    m->flush(m, frame);
  }
}

qbMemoryAllocator qb_memallocator_default() {
  return default_alloc;
}

struct qbMemoryPoolAllocator {
  // https://github.com/Isty001/mem-pool
  qbMemoryAllocator_ self;

  qbMemoryPoolAllocator() {
    pool_variable_init(&pool, 4096, 75);

    self.destroy = qbMemoryPoolAllocator::destroy;
    self.alloc = qbMemoryPoolAllocator::alloc;
    self.dealloc = qbMemoryPoolAllocator::dealloc;
    self.flush = nullptr;
    self.clear = nullptr;
  }

  static inline VariableMemPool* to_impl(qbMemoryAllocator_* self) {
    return ((qbMemoryPoolAllocator*)self)->pool;
  }

  static void destroy(qbMemoryAllocator_* self) {
    pool_variable_destroy(to_impl(self));
    delete ((qbMemoryPoolAllocator*)self);
  }

  static void* alloc(qbMemoryAllocator_* self, size_t size) {
    void* ret;
    pool_variable_alloc(to_impl(self), size, &ret);
    return ret;
  }

  static void dealloc(qbMemoryAllocator_* self, void* ptr) {
    pool_variable_free(to_impl(self), ptr);
  }

  VariableMemPool* pool;
};

qbMemoryAllocator qb_memallocator_pool() {
  return (qbMemoryAllocator)(new qbMemoryPoolAllocator());
}

///////////////////////////////////////////////////////////////////////////////
struct qbMemoryLinearAllocator {
  qbMemoryAllocator_ self;

  qbMemoryLinearAllocator(size_t max_size) {
    slab_ = new char[max_size];
    offset_ = 0;
    max_size_ = max_size;

    self.destroy = qbMemoryLinearAllocator::destroy;
    self.alloc = qbMemoryLinearAllocator::alloc;
    self.dealloc = qbMemoryLinearAllocator::dealloc;
    self.clear = qbMemoryLinearAllocator::clear;
    self.flush = nullptr;
  }

  static void destroy(qbMemoryAllocator_* allocator) {
    qbMemoryLinearAllocator* self = (qbMemoryLinearAllocator*)allocator;

    delete[] self->slab_;
    delete self;
  }

  static void* alloc(qbMemoryAllocator_* allocator, size_t size) {
    qbMemoryLinearAllocator* self = (qbMemoryLinearAllocator*)allocator;

    if (self->offset_ + size > self->max_size_) {
      return nullptr;
    }

    void* ret = ((qbMemoryLinearAllocator*)self)->slab_ + self->offset_;
    self->offset_ += size;
    return ret;
  }

  static void dealloc(qbMemoryAllocator_* allocator, void* ptr) {}

  static void clear(qbMemoryAllocator_* allocator) {
    qbMemoryLinearAllocator* self = (qbMemoryLinearAllocator*)allocator;
    self->offset_ = 0;
  }

  char* slab_;
  size_t offset_;
  size_t max_size_;
};

qbMemoryAllocator qb_memallocator_linear(size_t max_size) {
  return (qbMemoryAllocator)(new qbMemoryLinearAllocator(max_size));
}

///////////////////////////////////////////////////////////////////////////////
struct qbMemoryStackAllocator {
  qbMemoryAllocator_ self;

  struct Header {
    uint32_t size;
  };

  qbMemoryStackAllocator(size_t max_size) {
    slab_ = new char[max_size];
    offset_ = 0;
    max_size_ = max_size;

    self.destroy = qbMemoryStackAllocator::destroy;
    self.alloc = qbMemoryStackAllocator::alloc;
    self.dealloc = qbMemoryStackAllocator::dealloc;
    self.clear = qbMemoryStackAllocator::clear;
    self.flush = nullptr;
  }

  static void destroy(qbMemoryAllocator_* allocator) {
    qbMemoryStackAllocator* self = (qbMemoryStackAllocator*)allocator;

    delete[] self->slab_;
    delete self;
  }

  static void* alloc(qbMemoryAllocator_* allocator, size_t size) {
    qbMemoryStackAllocator* self = (qbMemoryStackAllocator*)allocator;
    size_t alloc_size = size + sizeof(Header);

    if (self->offset_ + alloc_size > self->max_size_) {
      return nullptr;
    }

    char* new_memory = ((qbMemoryStackAllocator*)self)->slab_ + self->offset_;
    void* ret = new_memory + sizeof(Header);
    ((Header*)new_memory)->size = alloc_size;
    
    self->offset_ += alloc_size;
    return ret;
  }

  static void dealloc(qbMemoryAllocator_* allocator, void* ptr) {
    qbMemoryStackAllocator* self = (qbMemoryStackAllocator*)allocator;   
    Header* header = (Header*)((char*)(ptr) - sizeof(Header));

    DEBUG_ASSERT((char*)header == self->slab_ + self->offset_ - header->size, -1);

    self->offset_ -= header->size;
  }

  static void clear(qbMemoryAllocator_* allocator) {
    qbMemoryStackAllocator* self = (qbMemoryStackAllocator*)allocator;
    self->offset_ = 0;
  }

  char* slab_;
  size_t offset_;
  size_t max_size_;
};

qbMemoryAllocator qb_memallocator_stack(size_t max_size) {
  return (qbMemoryAllocator)(new qbMemoryStackAllocator(max_size));
}