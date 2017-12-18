#include "component_registry.h"

#include "component.h"

ComponentRegistry::ComponentRegistry() : id_(0) {}

qbResult ComponentRegistry::CreateComponent(qbComponent* component, qbComponentAttr attr) {
  AllocComponent(component, attr);
  components_[(*component)->id] = *component;
  return qbResult::QB_OK;
}

qbHandle ComponentRegistry::CreateComponentInstance(
    qbComponent component, qbId entity_id, const void* value) {
  return component->impl->interface.insert(&component->impl->interface,
                                           &entity_id, (void*)value);
}

void ComponentRegistry::DestroyComponentInstance(qbComponent component, qbHandle handle) {
  component->impl->interface.remove_by_handle(&component->impl->interface,
                                              handle);
}

qbResult ComponentRegistry::SendComponentCreateEvent(qbComponent component,
                                            qbEntity entity,
                                            void* instance_data) {
  qbComponentOnCreateEvent_ event;
  event.entity = entity;
  event.component = component;
  event.instance_data = instance_data;

  return qb_event_send(component->on_create, &event);
}

qbResult ComponentRegistry::SendComponentDestroyEvent(qbComponent component,
                                             qbEntity entity,
                                             void* instance_data) {
  qbComponentOnDestroyEvent_ event;
  event.entity = entity;
  event.component = component;
  event.instance_data = instance_data;

  return qb_event_send(component->on_destroy, &event);
}

qbResult ComponentRegistry::AllocComponent(qbComponent* component, qbComponentAttr attr) {
  *component = (qbComponent)calloc(1, sizeof(qbComponent_));
  memset(*component, 0, sizeof(qbComponent_));

  *(qbId*)(&(*component)->id) = id_++;
  if (attr->impl) {
    (*component)->impl = attr->impl;
  } else {
    (*component)->impl = Component::new_collection(attr->program,
                                                   attr->data_size);
  }
  (*component)->data_size = attr->data_size;
  {
    qbEventAttr attr;
    qb_eventattr_create(&attr);
    qb_eventattr_setmessagetype(attr, qbComponentOnCreateEvent_);
    qb_event_create(&(*component)->on_create, attr);
    qb_eventattr_destroy(&attr);
  }
  {
    qbEventAttr attr;
    qb_eventattr_create(&attr);
    qb_eventattr_setmessagetype(attr, qbComponentOnDestroyEvent_);
    qb_event_create(&(*component)->on_destroy, attr);
    qb_eventattr_destroy(&attr);
  }
  return qbResult::QB_OK;
}

