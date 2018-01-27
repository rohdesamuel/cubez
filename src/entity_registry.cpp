#include "entity_registry.h"

#include <cstring>
#include <iostream>

EntityRegistry::EntityRegistry(ComponentRegistry* component_registry, FrameBuffer* frame_buffer)
    : id_(0), frame_buffer_(frame_buffer), component_registry_(component_registry) {
  entities_.reserve(100000);
  free_entity_ids_.reserve(10000);
}

void EntityRegistry::Init() {
}

EntityRegistry* EntityRegistry::Clone() {
  EntityRegistry* ret = new EntityRegistry(component_registry_, frame_buffer_);
  long id = id_;
  ret->id_ = id;
  ret->entities_ = entities_;
  ret->free_entity_ids_ = free_entity_ids_;
  return ret;
}

// Creates an entity. Entity will be available for use next frame. Sends a
// ComponentCreateEvent after all components have been created.
qbResult EntityRegistry::CreateEntity(qbEntity* entity,
                                      const qbEntityAttr_& attr) {
  qbId new_id = AllocEntity();

  LOG(INFO, "CreateEntity " << new_id << "\n");
  *entity = new_id;

  frame_buffer_->CreateEntity(*entity);
  component_registry_->CreateInstancesFor(*entity, attr.component_list);

  return qbResult::QB_OK;
}

// Destroys an entity and frees all components. Entity and components will be
// destroyed next frame. Sends a ComponentDestroyEvent before components are
// removed. Frees entity memory after all components have been destroyed.
qbResult EntityRegistry::DestroyEntity(qbEntity entity) {
  LOG(INFO, "Destroying instances for " << (entity) << "\n");
  component_registry_->DestroyInstancesFor(entity);

  LOG(INFO, "Send DestroyEntity Event for " << (entity) << "\n");
  frame_buffer_->DestroyEntity(entity);
 
  return QB_OK;
}

qbResult EntityRegistry::AddComponent(qbId entity, qbComponent component,
                                      void* instance_data) {
  return component_registry_->CreateInstanceFor(entity, component,
                                                instance_data);
}

qbResult EntityRegistry::RemoveComponent(qbId entity, qbComponent component) {
  return component_registry_->DestroyInstanceFor(entity, component);
}

qbResult EntityRegistry::Find(qbEntity* entity, qbId entity_id) {
  if (!entities_.has(entity_id)) {
    return QB_ERROR_NOT_FOUND;
  }
  *entity = entity_id;
  return QB_OK;
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
