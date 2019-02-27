#include "component_registry.h"

#include "component.h"
#include "game_state.h"
ComponentRegistry::ComponentRegistry() : id_(0) {}

ComponentRegistry::~ComponentRegistry() {}

ComponentRegistry* ComponentRegistry::Clone() {
  ComponentRegistry* ret = new ComponentRegistry();
  long id = id_;
  ret->id_ = id;
  ret->components_defs_ = components_defs_;
  return ret;
}

qbResult ComponentRegistry::Create(qbComponent* component, qbComponentAttr attr) {
  qbId new_id = id_++;
  components_defs_[new_id] = *attr;
  *component = new_id;

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

  return QB_OK;
}

Component* ComponentRegistry::Create(qbComponent component) const {
  const qbComponentAttr_& attr = components_defs_[component];
  return new Component(component, attr.data_size, attr.is_shared, attr.type);
}

qbResult ComponentRegistry::SubcsribeToOnCreate(qbSystem system,
                                                qbComponent component) {
  Create(component);
  return qb_event_subscribe(
    instance_create_events_[component], system);
}

qbResult ComponentRegistry::SubcsribeToOnDestroy(qbSystem system,
                                                 qbComponent component) {
  Create(component);
  return qb_event_subscribe(
    instance_destroy_events_[component], system);
}

qbResult ComponentRegistry::SendInstanceCreateNotification(
  qbEntity entity, Component* component, GameState* state) const {
  qbInstanceOnCreateEvent_ event;
  event.entity = entity;
  event.component = component;
  event.state = state;

  return qb_event_sendsync(
    instance_create_events_[component->Id()], &event);
}

qbResult ComponentRegistry::SendInstanceDestroyNotification(
  qbEntity entity, Component* component, GameState* state) const {
  qbInstanceOnDestroyEvent_ event;
  event.entity = entity;
  event.component = component;
  event.state = state;

  return qb_event_sendsync(
    instance_destroy_events_[component->Id()], &event);
}