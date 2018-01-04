#include "entity_registry.h"

#include <cstring>
#include <iostream>

EntityRegistry::EntityRegistry(ComponentRegistry* component_registry)
    : id_(0), component_registry_(component_registry) {
  entities_.reserve(100000);
  destroyed_entities_.reserve(10000);
  free_entity_ids_.reserve(10000);
}

void EntityRegistry::Init() {
  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_setcallback(attr, DestroyEntityHandler);
    qb_systemattr_settrigger(attr, qbTrigger::QB_TRIGGER_EVENT);
    qb_system_create(&destroy_entity_system_, attr);
    qb_systemattr_destroy(&attr);
  }
  {
    qbEventAttr attr;
    qb_eventattr_create(&attr);
    qb_eventattr_setmessagetype(attr, DestroyEntityEvent);
    qb_event_create(&destroy_entity_event_, attr);

    qb_event_subscribe(destroy_entity_event_, destroy_entity_system_);

    qb_eventattr_destroy(&attr);
  }

  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_setcallback(attr, CreateEntityHandler);
    qb_systemattr_settrigger(attr, qbTrigger::QB_TRIGGER_EVENT);
    qb_system_create(&create_entity_system_, attr);
    qb_systemattr_destroy(&attr);
  }
  {
    qbEventAttr attr;
    qb_eventattr_create(&attr);
    qb_eventattr_setmessagetype(attr, CreateEntityEvent);
    qb_event_create(&create_entity_event_, attr);

    qb_event_subscribe(create_entity_event_, create_entity_system_);

    qb_eventattr_destroy(&attr);
  }
}

// Creates an entity. Entity will be available for use next frame. Sends a
// ComponentCreateEvent after all components have been created.
qbResult EntityRegistry::CreateEntity(qbEntity* entity,
                                      const qbEntityAttr_& attr) {
  qbId new_id = AllocEntity();

  LOG(INFO, "CreateEntity " << new_id << "\n");
  *entity = new_id;

  CreateEntityEvent event;
  event.self = this;
  event.entity = new_id;
  qb_event_send(create_entity_event_, &event);

  component_registry_->CreateInstancesFor(*entity, attr.component_list);

  return qbResult::QB_OK;
}

// Destroys an entity and frees all components. Entity and components will be
// destroyed next frame. Sends a ComponentDestroyEvent before components are
// removed. Frees entity memory after all components have been destroyed.
qbResult EntityRegistry::DestroyEntity(qbEntity* entity) {
  std::lock_guard<std::mutex> lock(destroy_entity_mu_);
  
  // Only send events if the entity isn't destroyed yet.
  if (destroyed_entities_.has(*entity)) {
    return QB_OK;
  }
  destroyed_entities_.insert(*entity);

  LOG(INFO, "Destroying instances for " << (*entity) << "\n");
  component_registry_->DestroyInstancesFor(*entity);

  LOG(INFO, "Send DestroyEntity Event for " << (*entity) << "\n");
  SendDestroyEntityEvent(*entity);
 
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

qbResult EntityRegistry::SendDestroyEntityEvent(qbId entity) {
  DestroyEntityEvent event{this, entity};
  return qb_event_send(destroy_entity_event_, &event);
}

void EntityRegistry::CreateEntityHandler(qbFrame* frame) {
  CreateEntityEvent* event = (CreateEntityEvent*)frame->event;
  qbId new_id = event->entity;
  EntityRegistry* self = event->self;

  LOG(INFO, "CreateEntityHandler for " << event->entity << "\n");
  self->entities_.insert(new_id);
}

void EntityRegistry::DestroyEntityHandler(qbFrame* frame) {
  DestroyEntityEvent* event = (DestroyEntityEvent*)frame->event;
  EntityRegistry* self = event->self;

  LOG(INFO, "DestroyEntityHandler for " << event->entity << "\n");
  // Remove entity from index and make it available for use.
  self->entities_.erase(event->entity);
  self->destroyed_entities_.erase(event->entity);
  self->free_entity_ids_.push_back(event->entity);
}

