#include "entity_registry.h"

#include "component_registry.h"
#include <cstring>

EntityRegistry::EntityRegistry() : id_(0) {}

void EntityRegistry::Init() {
  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_setcallback(attr, AddComponentHandler);
    qb_systemattr_settrigger(attr, qbTrigger::EVENT);
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
    qb_systemattr_settrigger(attr, qbTrigger::EVENT);
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

  // Create the remove_component_event AFTER the destroy_entity_event to make
  // sure it is handled before the destroy_entity_event.
  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_setcallback(attr, RemoveComponentHandler);
    qb_systemattr_settrigger(attr, qbTrigger::EVENT);
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
  AllocEntity(entity);
  entities_[(*entity)->id] = *entity;
  SetComponents(*entity, attr.component_list);
  for (qbInstance_& instance : (*entity)->instances) {
    ComponentRegistry::SendComponentCreateEvent(
        instance.component, *entity, instance.data);
  }
  return qbResult::QB_OK;
}

// Destroys an entity and frees all components. Entity and components will be
// destroyed next frame. Sends a ComponentDestroyEvent before components are
// removed. Frees entity memory after all components have been destroyed.
qbResult EntityRegistry::DestroyEntity(qbEntity* entity) {
  for (qbInstance_& instance : (*entity)->instances) {
    SendRemoveComponentEvent(*entity, instance.component);
  }
  for (qbInstance_& instance : (*entity)->instances) {
    RemoveComponent(*entity, instance.component);
  }
  SendDestroyEntityEvent(*entity);
  *entity = nullptr;
  return QB_OK;
}

void EntityRegistry::SetComponents(qbEntity entity,
                    const std::vector<qbComponentInstance_>& components) {
  std::vector<qbInstance_> instances;
  for (const auto instance : components) {
    qbComponent component = instance.component;
    qbHandle handle = ComponentRegistry::CreateComponentInstance(
        component, entity->id, instance.data);

    instances.push_back({
        component,
        handle,
        component->impl->interface.by_handle(&component->impl->interface,
                                             handle)
      });
  }
  entity->instances = instances;
}

qbResult EntityRegistry::AddComponent(qbEntity entity, qbComponent component,
                   void* instance_data) {
  void* data = malloc(component->data_size);
  memcpy(data, instance_data, component->data_size);

  AddComponentEvent event{entity, component, data};
  qb_event_send(add_component_event_, &event);

  ComponentRegistry::SendComponentCreateEvent(component, entity, data);
  return QB_OK;
}

qbResult EntityRegistry::SendRemoveComponentEvent(qbEntity entity,
                                                  qbComponent component) {
  void* data = component->impl->interface.by_id(&component->impl->interface,
                                                entity->id);
  return ComponentRegistry::SendComponentDestroyEvent(component, entity, data);
}

qbResult EntityRegistry::RemoveComponent(qbEntity entity,
                                         qbComponent component) {
  RemoveComponentEvent event{entity, component};
  return qb_event_send(remove_component_event_, &event);
}

qbResult EntityRegistry::Find(qbEntity* entity, qbId entity_id) {
  auto it = entities_.find(entity_id);
  if (it == entities_.end()) {
    *entity = nullptr;
    return QB_ERROR_NOT_FOUND;
  }
  *entity = it->second;
  return QB_OK;
}

qbResult EntityRegistry::AllocEntity(qbEntity* entity) {
  *entity = (qbEntity)calloc(1, sizeof(qbEntity_));
  *(qbId*)(&(*entity)->id) = id_++;
  return qbResult::QB_OK;
}

void EntityRegistry::AddComponentHandler(qbCollectionInterface*,
                                         qbFrame* frame) {
  AddComponentEvent* event = (AddComponentEvent*)frame->event;
  qbEntity entity = event->entity;
  qbComponent component = event->component;
  void* instance_data = event->instance_data;

  std::vector<qbInstance_>* instances = &entity->instances;
  qbHandle handle = ComponentRegistry::CreateComponentInstance(component,
          entity->id,
          instance_data);
  instances->push_back({
      component,
      handle,
      component->impl->interface.by_handle(&component->impl->interface,
          handle)
      });
  free(instance_data);
}

void EntityRegistry::RemoveComponentHandler(qbCollectionInterface*,
                                     qbFrame* frame) {
  RemoveComponentEvent* event = (RemoveComponentEvent*)frame->event;
  qbEntity entity = event->entity;
  qbComponent component = event->component;

  std::vector<qbInstance_>* instances = &entity->instances;
  auto comp = [component](const qbInstance_& instance) {
        return instance.component == component;
      };
  auto to_erase = std::find_if(instances->begin(), instances->end(), comp);
  while(to_erase != instances->end()) {
    ComponentRegistry::DestroyComponentInstance(component,
        to_erase->instance_handle);
    instances->erase(to_erase);
    to_erase = std::find_if(instances->begin(), instances->end(), comp);
  } 
}

qbResult EntityRegistry::SendDestroyEntityEvent(qbEntity entity) {
  DestroyEntityEvent event{this, entity->id};
  return qb_event_send(destroy_entity_event_, &event);
}

void EntityRegistry::DestroyEntityHandler(qbCollectionInterface*,
                                          qbFrame* frame) {
  DestroyEntityEvent* event = (DestroyEntityEvent*)frame->event;
  EntityRegistry* self = event->self;

  auto it = self->entities_.find(event->entity_id);
  if (it != self->entities_.end()) {
    qbEntity entity = it->second;
    free(entity);
    self->entities_.erase(it);
  }
}

