#include "entity_registry.h"

#include <cstring>
#include <iostream>

#include "game_state.h"
EntityRegistry::EntityRegistry()
    : id_(0) {
  entities_.reserve(100000);
  free_entity_ids_.reserve(10000);
}

void EntityRegistry::Init() {
}

EntityRegistry* EntityRegistry::Clone() {
  EntityRegistry* ret = new EntityRegistry();
  long id = id_;
  ret->id_ = id;
  ret->entities_ = entities_;
  ret->free_entity_ids_ = free_entity_ids_;
  return ret;
}

// Creates an entity. Entity will be available for use next frame. Sends a
// ComponentCreateEvent after all components have been created.
qbResult EntityRegistry::CreateEntity(qbEntity* entity,
                                      const qbEntityAttr_& /** attr */) {
  qbId new_id = AllocEntity();
  entities_.insert(new_id);

  LOG(INFO, "CreateEntity " << new_id << "\n");
  *entity = new_id;

  return qbResult::QB_OK;
}

// Destroys an entity and frees all components. Entity and components will be
// destroyed next frame. Sends a ComponentDestroyEvent before components are
// removed. Frees entity memory after all components have been destroyed.
qbResult EntityRegistry::DestroyEntity(qbEntity entity) {
  LOG(INFO, "Destroying instances for " << (entity) << "\n");
  if (entities_.has(entity)) {
    entities_.erase(entity);
    free_entity_ids_.push_back(entity);
  }
  return QB_OK;
}

qbResult EntityRegistry::Find(qbEntity entity, qbEntity* found) {
  if (!entities_.has(entity)) {
    return QB_ERROR_NOT_FOUND;
  }
  *found = entity;
  return QB_OK;
}

bool EntityRegistry::Has(qbEntity entity) {
  return entities_.has(entity);
}

void EntityRegistry::Resolve(const std::vector<qbEntity>& created,
                             const std::vector<qbEntity>& destroyed) {
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

qbId EntityRegistry::AllocEntity() {
  qbId new_id;
  if (free_entity_ids_.empty()) {
    new_id = id_++;
  } else {
    new_id = free_entity_ids_.back();
    free_entity_ids_.pop_back();
  }

  return new_id;
}
