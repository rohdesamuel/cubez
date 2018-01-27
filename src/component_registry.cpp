#include "component_registry.h"

#include "component.h"

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

qbResult ComponentRegistry::Create(qbComponent* component, qbComponentAttr attr,
                                   FrameBuffer* frame_buffer) {
  qbId new_id = id_++;

  components_[new_id] = new Component(frame_buffer, new_id, attr->data_size);
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
  *component = (qbComponent)(components_[new_id]);
  return qbResult::QB_OK;
}

qbResult ComponentRegistry::CreateInstancesFor(
    qbEntity entity, const std::vector<qbComponentInstance_>& instances) {
  for (auto& instance : instances) {
    instance.component->instances.Create(entity, instance.data);
  }

  for (auto& instance : instances) {
    SendInstanceCreateNotification(entity, instance.component);
  }

  return QB_OK;
}

qbResult ComponentRegistry::CreateInstanceFor(qbEntity entity,
                                              qbComponent component,
                                              void* instance_data) {
  component->instances.Create(entity, instance_data);
  SendInstanceCreateNotification(entity, component);
  return QB_OK;
}

int ComponentRegistry::DestroyInstancesFor(qbEntity entity) {
  int destroyed_instances = 0;
  for (auto it = components_.begin(); it != components_.end(); ++it) {
    qbComponent component = (qbComponent)(*it).second;
    if (component->instances.Has(entity)) {
      SendInstanceDestroyNotification(entity, component);
    }
  }

  for (auto it = components_.begin(); it != components_.end(); ++it) {
    qbComponent component = (qbComponent)(*it).second;
    if (component->instances.Has(entity)) {
      component->instances.Destroy(entity);
      ++destroyed_instances;
    }
  }
  return destroyed_instances;
}

qbResult ComponentRegistry::DestroyInstanceFor(qbEntity entity,
                                               qbComponent component) {
  SendInstanceDestroyNotification(entity, component);
  component->instances.Destroy(entity);
  return QB_OK;
}

qbResult ComponentRegistry::SendInstanceCreateNotification(
    qbEntity entity, qbComponent component) {
  qbInstanceOnCreateEvent_ event;
  event.entity = entity;
  event.component = component;

  return qb_event_sendsync(
      instance_create_events_[component->instances.Id()], &event);
}

qbResult ComponentRegistry::SendInstanceDestroyNotification(
    qbEntity entity, qbComponent component) {
  qbInstanceOnDestroyEvent_ event;
  event.entity = entity;
  event.component = component;

  return qb_event_sendsync(
      instance_destroy_events_[component->instances.Id()], &event);
}

qbResult ComponentRegistry::SubcsribeToOnCreate(qbSystem system,
                                                qbComponent component) {
  return qb_event_subscribe(
      instance_create_events_[component->instances.Id()], system);
}

qbResult ComponentRegistry::SubcsribeToOnDestroy(qbSystem system,
                                                 qbComponent component) {
  return qb_event_subscribe(
      instance_destroy_events_[component->instances.Id()], system);
}