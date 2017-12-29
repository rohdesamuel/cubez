#include "entity_registry.h"

#include <cstring>
#include <iostream>

const uint32_t kMaxInstanceCount = 5;

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
    qb_systemattr_setcallback(attr, AddComponentHandler);
    qb_systemattr_settrigger(attr, qbTrigger::QB_TRIGGER_EVENT);
    qb_systemattr_setuserstate(attr, this);
    qb_system_create(&add_component_system_, attr);
    qb_systemattr_destroy(&attr);
  }
  {
    qbEventAttr attr;
    qb_eventattr_create(&attr);
    qb_eventattr_setmessagetype(attr, AddComponentEvent);
    qb_event_create(&add_component_event_, attr);
    
    qb_event_subscribe(add_component_event_, add_component_system_);

    qb_eventattr_destroy(&attr);
  }

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
    qb_systemattr_setcallback(attr, RemoveComponentHandler);
    qb_systemattr_settrigger(attr, qbTrigger::QB_TRIGGER_EVENT);
    qb_systemattr_setuserstate(attr, this);
    qb_system_create(&remove_component_system_, attr);
    qb_systemattr_destroy(&attr);
  }
  {
    qbEventAttr attr;
    qb_eventattr_create(&attr);
    qb_eventattr_setmessagetype(attr, RemoveComponentEvent);
    qb_event_create(&remove_component_event_, attr);

    qb_event_subscribe(remove_component_event_, remove_component_system_);

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
  SetComponents(new_id, attr.component_list);

  for (const qbComponentInstance_& instance: attr.component_list) {
    qbComponent component = instance.component;
    ComponentRegistry::SendComponentCreateEvent(
      component, new_id, component->instances[*entity]);
  }
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

  LOG(INFO, "Send OnRemoveComponent Event for " << (*entity) << "\n");
  
  for (auto it = component_registry_->begin(); it != component_registry_->end(); ++it) {
    qbComponent component = (*it).second;
    if (component->instances.has(*entity)) {
      SendRemoveComponentEvent(*entity, component);
    }
  }

  LOG(INFO, "Send RemoveComponent Event for " << (*entity) << "\n");
  for (auto it = component_registry_->begin(); it != component_registry_->end(); ++it) {
    qbComponent component = (*it).second;
    if (component->instances.has(*entity)) {
      RemoveComponent(*entity, component);
    }
  }

  LOG(INFO, "Send DestroyEntity Event for " << (*entity) << "\n");
  SendDestroyEntityEvent(*entity);
 
  return QB_OK;
}

void EntityRegistry::SetComponents(qbId entity,
                                   const std::vector<qbComponentInstance_>& components) {
  for (const auto& instance : components) {
    qbComponent component = instance.component;
    ComponentRegistry::CreateComponentInstance(component, entity, instance.data);
  }
}

qbResult EntityRegistry::AddComponent(qbId entity, qbComponent component,
                                      void* instance_data) {
  size_t element_size = component->instances.element_size();
  void* data = malloc(element_size);
  memcpy(data, instance_data, element_size);

  AddComponentEvent event{entity, component, data};
  qb_event_send(add_component_event_, &event);

  component_registry_->SendComponentCreateEvent(component, entity, data);
  return QB_OK;
}

qbResult EntityRegistry::SendRemoveComponentEvent(qbId entity,
                                                  qbComponent component) {
  void* data = component->instances[entity];
  return component_registry_->SendComponentDestroyEvent(component, entity, data);
}

qbResult EntityRegistry::RemoveComponent(qbId entity,
                                         qbComponent component) {
  RemoveComponentEvent event{entity, component};
  return qb_event_send(remove_component_event_, &event);
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

  entities_.insert(new_id);

  return new_id;
}

void EntityRegistry::AddComponentHandler(qbFrame* frame) {
  AddComponentEvent* event = (AddComponentEvent*)frame->event;
  qbId entity = event->entity;
  qbComponent component = event->component;

  component->instances.insert(entity, event->instance_data);

  free(event->instance_data);
}

void EntityRegistry::RemoveComponentHandler(qbFrame* frame) {
  RemoveComponentEvent* event = (RemoveComponentEvent*)frame->event;
  qbId entity = event->entity;
  qbComponent component = event->component;

  LOG(INFO, "RemoveComponent for " << entity << "\n");

  component->instances.erase(entity);
}

qbResult EntityRegistry::SendDestroyEntityEvent(qbId entity) {
  DestroyEntityEvent event{this, entity};
  return qb_event_send(destroy_entity_event_, &event);
}

void EntityRegistry::DestroyEntityHandler(qbFrame* frame) {
  DestroyEntityEvent* event = (DestroyEntityEvent*)frame->event;
  EntityRegistry* self = event->self;

  LOG(INFO, "DestroyEntity for " << event->entity << "\n");
  // Remove entity from index and make it available for use.
  self->entities_.erase(event->entity);
  self->destroyed_entities_.erase(event->entity);
  self->free_entity_ids_.push_back(event->entity);
}

