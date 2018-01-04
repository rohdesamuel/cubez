#ifndef COMPONENT__H
#define COMPONENT__H

#include "cubez.h"
#include "sparse_map.h"

class Component {
 public:
  typedef typename SparseMap<void>::iterator iterator;

  Component(qbComponent parent, qbId id, size_t instance_size);

  qbResult Create(qbId entity, void* value);
  qbResult Destroy(qbId entity);

  qbResult SendInstanceCreateNotification(qbEntity entity);
  qbResult SendInstanceDestroyNotification(qbEntity entity);

  qbResult SubcsribeToOnCreate(qbSystem system);
  qbResult SubcsribeToOnDestroy(qbSystem system);

  void* operator[](qbId entity);
  const void* operator[](qbId entity) const;

  bool Has(qbId entity);

  bool Empty();
  size_t Size();
  void Reserve(size_t count);

  size_t ElementSize();

  iterator begin();
  iterator end();

 private:
  qbComponent parent_;
  qbId id_;

  SparseMap<void> instances_;
  qbEvent on_create_;
  qbEvent on_destroy_;
};

#endif
