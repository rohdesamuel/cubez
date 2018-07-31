#ifndef COMPONENT__H
#define COMPONENT__H

#include "cubez.h"
#include "sparse_map.h"
#include "sparse_set.h"

#include <shared_mutex>

// Not thread-safe. 
class Component {
  typedef SparseMap<void, BlockVector> InstanceMap;
 public:
  typedef typename InstanceMap::iterator iterator;
  typedef typename InstanceMap::const_iterator const_iterator;

  Component(qbId id, size_t instance_size, bool is_shared);

  Component* Clone();
  void Merge(const Component& other);

  qbResult Create(qbId entity, void* value);
  qbResult Destroy(qbId entity);

  void* operator[](qbId entity);
  const void* operator[](qbId entity) const;
  const void* at(qbId entity) const;

  bool Has(qbId entity) const;

  bool Empty() const;
  size_t Size() const;
  void Reserve(size_t count);

  size_t ElementSize() const;
  qbId Id() const;

  void Lock(bool is_mutable=false);
  void Unlock(bool is_mutable = false);

  iterator begin();
  iterator end();

  const_iterator begin() const;
  const_iterator end() const;

 private:
  qbId id_;
  InstanceMap instances_;

  std::shared_mutex mu_;
  const bool is_shared_;
};

#endif
