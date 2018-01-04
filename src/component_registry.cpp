#include "component_registry.h"

#include "component.h"

ComponentRegistry::ComponentRegistry() : id_(0) {}

void ComponentRegistry::Init() {
  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_setcallback(attr, InstanceDestroyHandler);
    qb_systemattr_settrigger(attr, qbTrigger::QB_TRIGGER_EVENT);
    qb_systemattr_setuserstate(attr, this);
    qb_system_create(&instance_destroy_system_, attr);
    qb_systemattr_destroy(&attr);
  }
  {
    qbEventAttr attr;
    qb_eventattr_create(&attr);
    qb_eventattr_setmessagetype(attr, InstanceDestroyEvent);
    qb_event_create(&instance_destroy_event_, attr);

    qb_event_subscribe(instance_destroy_event_, instance_destroy_system_);

    qb_eventattr_destroy(&attr);
  }

  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_setcallback(attr, InstanceCreateHandler);
    qb_systemattr_settrigger(attr, qbTrigger::QB_TRIGGER_EVENT);
    qb_systemattr_setuserstate(attr, this);
    qb_system_create(&instance_create_system_, attr);
    qb_systemattr_destroy(&attr);
  }
  {
    qbEventAttr attr;
    qb_eventattr_create(&attr);
    qb_eventattr_setmessagetype(attr, InstanceCreateEvent);
    qb_event_create(&instance_create_event_, attr);
    
    qb_event_subscribe(instance_create_event_, instance_create_system_);

    qb_eventattr_destroy(&attr);
  }
}

qbResult ComponentRegistry::ComponentCreate(qbComponent* component, qbComponentAttr attr) {
  qbId new_id = id_++;
  *component = (qbComponent)malloc(sizeof(qbComponent_));
  new (*component) qbComponent_{Component(*component, new_id, attr->data_size)};

  components_[new_id] = *component;
  components_[new_id]->instances.Reserve(10000);
  *component = components_[new_id];
  return qbResult::QB_OK;
}

void ComponentRegistry::InstanceCreateHandler(qbFrame* frame) {
  InstanceCreateEvent* event = (InstanceCreateEvent*)frame->event;
  qbId entity = event->entity;
  qbComponent component = event->component;

  component->instances.Create(entity, event->instance_data);

  free(event->instance_data);
}

qbResult ComponentRegistry::CreateInstancesFor(
    qbEntity entity, const std::vector<qbComponentInstance_>& instances) {
  for (auto& instance : instances) {
    void* instance_data = malloc(instance.component->instances.ElementSize());
    memmove(instance_data, instance.data,
            instance.component->instances.ElementSize());

    InstanceCreateEvent event;
    event.entity = entity;
    event.component = instance.component;
    event.instance_data = instance_data;
    qb_event_send(instance_create_event_, &event);
  }

  for (auto& instance : instances) {
    instance.component->instances.SendInstanceCreateNotification(entity);
  }

  return QB_OK;
}

qbResult ComponentRegistry::CreateInstanceFor(qbEntity entity,
                                              qbComponent component,
                                              void* instance_data) {
  void* data = malloc(component->instances.ElementSize());
  memmove(instance_data, instance_data, component->instances.ElementSize());

  InstanceCreateEvent event;
  event.entity = entity;
  event.component = component;
  event.instance_data = data;
  qb_event_send(instance_create_event_, &event);

  component->instances.SendInstanceCreateNotification(entity);
  return QB_OK;
}

int ComponentRegistry::DestroyInstancesFor(qbEntity entity) {
  int destroyed_instances = 0;
  for (auto it = components_.begin(); it != components_.end(); ++it) {
    qbComponent component = (*it).second;
    if (component->instances.Has(entity)) {
      component->instances.SendInstanceDestroyNotification(entity);
    }
  }

  for (auto it = components_.begin(); it != components_.end(); ++it) {
    qbComponent component = (*it).second;
    if (component->instances.Has(entity)) {
      InstanceDestroyEvent event{entity, component};
      destroyed_instances +=
          qb_event_send(instance_destroy_event_, &event) == QB_OK;
    }
  }
  return destroyed_instances;
}

qbResult ComponentRegistry::DestroyInstanceFor(qbEntity entity,
                                               qbComponent component) {
  InstanceDestroyEvent event{entity, component};
  return qb_event_send(instance_destroy_event_, &event);
}

void ComponentRegistry::InstanceDestroyHandler(qbFrame* frame) {
  InstanceDestroyEvent* e = (InstanceDestroyEvent*)frame->event;
  e->component->instances.Destroy(e->entity);
}
