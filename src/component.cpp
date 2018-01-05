#include "component.h"
#include "defs.h"

Component::Component(qbComponent parent, qbId id, size_t instance_size)
    : parent_(parent), id_(id), instances_(instance_size) {
  {
    qbEventAttr attr;
    qb_eventattr_create(&attr);
    qb_eventattr_setmessagetype(attr, qbInstanceOnCreateEvent_);
    qb_event_create(&on_create_, attr);
    qb_eventattr_destroy(&attr);
  }
  {
    qbEventAttr attr;
    qb_eventattr_create(&attr);
    qb_eventattr_setmessagetype(attr, qbInstanceOnDestroyEvent_);
    qb_event_create(&on_destroy_, attr);
    qb_eventattr_destroy(&attr);
  }
}

qbResult Component::Create(qbId entity, void* value) {
  instances_.insert(entity, value);

  qbInstanceOnCreateEvent_ event;
  event.entity = entity;
  event.component = parent_;

  return qb_event_send(on_create_, &event);
}

qbResult Component::Destroy(qbId entity) {
  instances_.erase(entity);
  return QB_OK;
}

qbResult Component::SubcsribeToOnCreate(qbSystem system) {
  return qb_event_subscribe(on_create_, system);
}

qbResult Component::SubcsribeToOnDestroy(qbSystem system) {
  return qb_event_subscribe(on_destroy_, system);
}

void* Component::operator[](qbId entity) {
  return instances_[entity];
}

const void* Component::operator[](qbId entity) const {
  return instances_[entity];
}

bool Component::Has(qbId entity) {
  return instances_.has(entity);
}

bool Component::Empty() {
  return instances_.size() == 0;
}

size_t Component::Size() {
  return instances_.size();
}

size_t Component::ElementSize() {
  return instances_.element_size();
}

void Component::Reserve(size_t count) {
  return instances_.reserve(count);
}

qbResult Component::SendInstanceCreateNotification(qbEntity entity) {
  qbInstanceOnCreateEvent_ event;
  event.entity = entity;
  event.component = parent_;

  return qb_event_send(on_create_, &event);
}

qbResult Component::SendInstanceDestroyNotification(qbEntity entity) {
  qbInstanceOnDestroyEvent_ event;
  event.entity = entity;
  event.component = parent_;

  return qb_event_send(on_destroy_, &event);
}


Component::iterator Component::begin() {
  return instances_.begin();
}

Component::iterator Component::end() {
  return instances_.end();
}
