#ifndef ENTITY_REGISTRY__H
#define ENTITY_REGISTRY__H

#include "defs.h"
#include "memory_pool.h"
#include "component_registry.h"
#include "sparse_set.h"

#include <algorithm>
#include <atomic>
#include <unordered_map>
#include <unordered_set>

class GameState;
class EntityRegistry {
 public:
  typedef SparseSet::iterator iterator;
  typedef SparseSet::const_iterator const_iterator;

  EntityRegistry();

  void Init();
  EntityRegistry* Clone();

  // Creates an entity. Entity will be available for use next frame. Sends a
  // ComponentCreateEvent after all components have been created.
  qbResult CreateEntity(qbEntity* entity, const qbEntityAttr_& attr);

  // Destroys an entity and frees all components. Entity and components will be
  // destroyed next frame. Sends a ComponentDestroyEvent before components are
  // removed. Frees entity memory after all components have been destroyed.
  qbResult DestroyEntity(qbEntity entity);

  qbResult Find(qbEntity entity, qbEntity* found);

  iterator begin() {
    return entities_.begin();
  }

  const_iterator begin() const {
    return entities_.begin();
  }

  iterator end() {
    return entities_.end();
  }

  const_iterator end() const {
    return entities_.end();
  }

  bool Has(qbEntity entity);

  void Resolve(const std::vector<qbEntity>& created,
               const std::vector<qbEntity>& destroyed);

  template<template<class Ty_> class Container_>
  void Resolve(const Container_<qbEntity>& created,
               const Container_<qbEntity>& destroyed) {
    for (qbEntity entity : destroyed) {
      if (entities_.has(entity)) {
        entities_.erase(entity);
        free_entity_ids_.push_back(entity);
      }
    }
    for (qbEntity entity : created) {
      entities_.insert(entity);
    }
  }

 private:
  qbId AllocEntity();

  std::atomic_long id_;
  SparseSet entities_;
  std::vector<size_t> free_entity_ids_;

};

#endif  // ENTITY_REGISTRY__H

