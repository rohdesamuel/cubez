#include "component_registry.h"

#include "component.h"

ComponentRegistry::ComponentRegistry() : id_(0) {}

qbResult ComponentRegistry::CreateComponent(qbComponent* component, qbComponentAttr attr) {
  qbId new_id = id_++;
  components_[new_id] =
      new qbComponent_{ new_id, SparseMap<void>{attr->data_size}, nullptr,
                        nullptr };
  components_[new_id]->instances.reserve(10000);
  *component = components_[new_id];
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

qbId ComponentRegistry::CreateComponentInstance(
    qbComponent component, qbId entity_id, const void* value) {
  component->instances.insert(entity_id, (void*)value);
  return entity_id;
}

void ComponentRegistry::DestroyComponentInstance(qbComponent component, qbId id) {
  return component->instances.erase(id);
}

qbResult ComponentRegistry::SendComponentCreateEvent(qbComponent component,
                                                     qbId entity,
                                                     void* instance_data) {
  qbComponentOnCreateEvent_ event;
  event.entity = entity;
  event.component = component;
  event.instance_data = instance_data;

  return qb_event_send(component->on_create, &event);
}

qbResult ComponentRegistry::SendComponentDestroyEvent(qbComponent component,
                                                      qbId entity,
                                                      void* instance_data) {
  qbComponentOnDestroyEvent_ event;
  event.entity = entity;
  event.component = component;
  event.instance_data = instance_data;

  return qb_event_send(component->on_destroy, &event);
}
