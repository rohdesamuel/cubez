#include <cubez/memory.h>

#include "mem-pool/mem_pool.h"

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