#include "component_registry.h"

#include "component.h"
#include "game_state.h"
ComponentRegistry::ComponentRegistry() : id_(0) {}

ComponentRegistry::~ComponentRegistry() {
  for (auto c_pair : components_) {
    delete c_pair.second;
  }
}

void ComponentRegistry::Init() {}

ComponentRegistry* ComponentRegistry::Clone() {
  ComponentRegistry* ret = new ComponentRegistry();
  long id = id_;
  ret->id_ = id;
  for (auto c_pair : components_) {
    ret->components_[c_pair.first] = c_pair.second->Clone();
  }
  return ret;
}

qbResult ComponentRegistry::Create(qbComponent* component, qbComponentAttr attr) {
  qbId new_id = id_++;

  *component = new_id;
  components_[new_id] = new Component(new_id, attr->data_size, attr->is_shared);
  {
    instance_create_events_.resize(new_id + 1);
    qbEvent& on_create = instance_create_events_[new_id];

    qbEventAttr attr;
    qb_eventattr_create(&attr);
    qb_eventattr_setmessagetype(attr, qbInstanceOnCreateEvent_);
    qb_event_create(&on_create, attr);
    qb_eventattr_destroy(&attr);
  }
  {
    instance_destroy_events_.resize(new_id + 1);
    qbEvent& on_destroy = instance_destroy_events_[new_id];

    qbEventAttr attr;
    qb_eventattr_create(&attr);
    qb_eventattr_setmessagetype(attr, qbInstanceOnDestroyEvent_);
    qb_event_create(&on_destroy, attr);
    qb_eventattr_destroy(&attr);
  }
  
  return qbResult::QB_OK;
}

qbResult ComponentRegistry::CreateInstancesFor(
    qbEntity entity, const std::vector<qbComponentInstance_>& instances,
    GameState* state) {
  for (auto& instance : instances) {
    Component* component = components_[instance.component];
    component->Create(entity, instance.data);
  }

  for (auto& instance : instances) {
    Component* component = components_[instance.component];
    SendInstanceCreateNotification(entity, component, state);
  }

  return QB_OK;
}

qbResult ComponentRegistry::CreateInstanceFor(qbEntity entity,
                                              qbComponent component,
                                              void* instance_data,
                                              GameState* state) {
  Component* c = components_[component];
  c->Create(entity, instance_data);
  SendInstanceCreateNotification(entity, c, state);
  return QB_OK;
}

int ComponentRegistry::DestroyInstancesFor(qbEntity entity, GameState* state) {
  int destroyed_instances = 0;
  for (auto component_pair : components_) {
    Component* component = component_pair.second;
    if (component->Has(entity)) {
      SendInstanceDestroyNotification(entity, component, state);
    }
  }

  for (auto component_pair : components_) {
    Component* component = component_pair.second;
    if (component->Has(entity)) {
      component->Destroy(entity);
      ++destroyed_instances;
    }
  }
  return destroyed_instances;
}

int ComponentRegistry::DestroyInstanceFor(qbEntity entity,
                                          qbComponent component,
                                          GameState* state) {
  Component* c = components_[component];
  if (c->Has(entity)) {
    SendInstanceDestroyNotification(entity, c, state);
    c->Destroy(entity);
    return 1;
  }
  return 0;
}

qbResult ComponentRegistry::SendInstanceCreateNotification(
    qbEntity entity, Component* component, GameState* state) {
  qbInstanceOnCreateEvent_ event;
  event.entity = entity;
  event.component = component;
  event.state = state;

  return qb_event_sendsync(
      instance_create_events_[component->Id()], &event);
}

qbResult ComponentRegistry::SendInstanceDestroyNotification(
    qbEntity entity, Component* component, GameState* state) {
  qbInstanceOnDestroyEvent_ event;
  event.entity = entity;
  event.component = component;
  event.state = state;

  return qb_event_sendsync(
      instance_destroy_events_[component->Id()], &event);
}

qbResult ComponentRegistry::SubcsribeToOnCreate(qbSystem system,
                                                qbComponent component) const {
  return qb_event_subscribe(
      instance_create_events_[component], system);
}

qbResult ComponentRegistry::SubcsribeToOnDestroy(qbSystem system,
                                                 qbComponent component) const {
  return qb_event_subscribe(
      instance_destroy_events_[component], system);
}