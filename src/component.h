#ifndef COMPONENT__H
#define COMPONENT__H

#include "cubez.h"
#include "sparse_map.h"
#include "sparse_set.h"

class FrameBuffer;
class Component {
  typedef SparseMap<void, ByteVector> InstanceMap;
 public:
  typedef typename InstanceMap::iterator iterator;
  typedef typename InstanceMap::const_iterator const_iterator;

  Component(FrameBuffer* buffer, qbId id, size_t instance_size);

  class ComponentBuffer* MakeBuffer();

  Component* Clone();
  void Merge(ComponentBuffer* update);

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

  iterator begin();
  iterator end();

  const_iterator begin() const;
  const_iterator end() const;

 private:
  FrameBuffer* frame_buffer_;
  qbId id_;
  InstanceMap instances_;
};

class ComponentBuffer {
public:
  ComponentBuffer(Component* component);

  void* FindOrCreate(qbId entity, void* value);

  const void* Find(qbId entity);

  qbResult Create(qbId entity, void* value);

  qbResult Destroy(qbId entity);

  bool Has(qbId entity);

  void Resolve();

  void Clear();

private:
  SparseMap<void, BlockVector> insert_or_update_;
  SparseSet destroyed_;
  Component* component_;

  friend class Component;
};

#endif
