#include "component.h"
#include "defs.h"
#include "frame_buffer.h"

Component::Component(FrameBuffer* buffer, qbComponent parent, qbId id, size_t instance_size)
    : frame_buffer_(buffer), parent_(parent), id_(id), instances_(instance_size) {
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

ComponentBuffer* Component::MakeBuffer() {
  return new ComponentBuffer{ this };
}

void Component::Merge(ComponentBuffer* update) {
  for (auto pair : update->insert_or_update_) {
    memmove(instances_[pair.first], pair.second, ElementSize());
  }
  for (auto id : update->destroyed_) {
    if (instances_.has(id)) {
      instances_.erase(id);
    }
  }
}

qbResult Component::Create(qbId entity, void* value) {
  return frame_buffer_->Component(id_)->Create(entity, value);
}

qbResult Component::Destroy(qbId entity) {
  return frame_buffer_->Component(id_)->Destroy(entity);
}

qbResult Component::SubcsribeToOnCreate(qbSystem system) {
  return qb_event_subscribe(on_create_, system);
}

qbResult Component::SubcsribeToOnDestroy(qbSystem system) {
  return qb_event_subscribe(on_destroy_, system);
}

void* Component::operator[](qbId entity) {
  void* result = frame_buffer_->Component(id_)->FindOrCreate(entity, instances_[entity]);
  if (result) {
    return result;
  }
  return instances_[entity];
}

const void* Component::operator[](qbId entity) const {
  const void* result = frame_buffer_->Component(id_)->Find(entity);
  if (result) {
    return result;
  }
  return instances_[entity];
}

const void* Component::at(qbId entity) const {
  return (*this)[entity];
}

bool Component::Has(qbId entity) const {
  if (instances_.has(entity)) {
    return true;
  }
  return frame_buffer_->Component(id_)->Has(entity);
}

bool Component::Empty() const {
  return instances_.size() == 0;
}

size_t Component::Size() const {
  return instances_.size();
}

size_t Component::ElementSize() const {
  return instances_.element_size();
}

void Component::Reserve(size_t count) {
  return instances_.reserve(count);
}

qbResult Component::SendInstanceCreateNotification(qbEntity entity) {
  qbInstanceOnCreateEvent_ event;
  event.entity = entity;
  event.component = parent_;

  return qb_event_sendsync(on_create_, &event);
}

qbResult Component::SendInstanceDestroyNotification(qbEntity entity) {
  qbInstanceOnDestroyEvent_ event;
  event.entity = entity;
  event.component = parent_;

  return qb_event_sendsync(on_destroy_, &event);
}

qbId Component::Id() const {
  return id_;
}

Component::iterator Component::begin() {
  return instances_.begin();
}

Component::iterator Component::end() {
  return instances_.end();
}

ComponentBuffer::ComponentBuffer(Component* component)
  : insert_or_update_(component->ElementSize()), component_(component) {}

void* ComponentBuffer::FindOrCreate(qbId entity, void* value) {
  if (!insert_or_update_.has(entity)) {
    insert_or_update_.insert(entity, value);
  }
  return insert_or_update_[entity];
}

const void* ComponentBuffer::Find(qbId entity) {
  if (insert_or_update_.has(entity)) {
    return insert_or_update_[entity];
  }
  return nullptr;
}

qbResult ComponentBuffer::Create(qbId entity, void* value) {
  insert_or_update_.insert(entity, value);
  return QB_OK;
}

qbResult ComponentBuffer::Destroy(qbId entity) {
  destroyed_.insert(entity);
  return QB_OK;
}

bool ComponentBuffer::Has(qbId entity) {
  return destroyed_.has(entity) ? false : insert_or_update_.has(entity);
}

void ComponentBuffer::Clear() {
  insert_or_update_.clear();
  destroyed_.clear();
}

void ComponentBuffer::Resolve() {
  component_->Merge(this);
  Clear();
}