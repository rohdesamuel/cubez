#ifndef COMPONENT__H
#define COMPONENT__H

#include "cubez.h"
#include "sparse_map.h"
#include "sparse_set.h"

class FrameBuffer;
class Component {
 public:
  typedef typename SparseMap<void>::iterator iterator;

  Component(FrameBuffer* buffer, qbComponent parent, qbId id, size_t instance_size);
  class ComponentBuffer* MakeBuffer();

  void Merge(ComponentBuffer* update);

  qbResult Create(qbId entity, void* value);
  qbResult Destroy(qbId entity);

  qbResult SendInstanceCreateNotification(qbEntity entity);
  qbResult SendInstanceDestroyNotification(qbEntity entity);

  qbResult SubcsribeToOnCreate(qbSystem system);
  qbResult SubcsribeToOnDestroy(qbSystem system);

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

 private:
  FrameBuffer* frame_buffer_;

  qbComponent parent_;
  qbId id_;
  
  SparseMap<void> instances_;

  qbEvent on_create_;
  qbEvent on_destroy_;
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
  SparseMap<void> insert_or_update_;
  SparseSet destroyed_;
  Component* component_;

  friend class Component;
};

#endif
