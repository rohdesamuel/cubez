#include "apex_memmove.h"
#include "component.h"
#include "defs.h"
#include "frame_buffer.h"

#include <omp.h>

Component::Component(FrameBuffer* buffer, qbId id, size_t instance_size)
    : frame_buffer_(buffer), id_(id), instances_(instance_size) {}

ComponentBuffer* Component::MakeBuffer() {
  return new ComponentBuffer{ this };
}

Component* Component::Clone() {
  Component* ret = new Component(frame_buffer_, id_, instances_.element_size());
  *ret = *this;
  return ret;
}

void Component::Merge(ComponentBuffer* update) {
  instances_.reserve(update->insert_or_update_.capacity());

  for (auto pair : update->insert_or_update_) {
    if (!instances_.has(pair.first)) {
      instances_.insert(pair.first, nullptr);
    }
  }

#pragma omp parallel for
  for (int64_t i = 0; i < update->insert_or_update_.size(); ++i) {
    auto it = update->insert_or_update_.begin() + i;
    apex::memmove(instances_[(*it).first], (*it).second, ElementSize());
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

void* Component::operator[](qbId entity) {
  void* result = frame_buffer_->Component(id_)->FindOrCreate(entity, instances_[entity]);
  if (result) {
    return result;
  }
  return instances_[entity];
}

const void* Component::operator[](qbId entity) const {
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

qbId Component::Id() const {
  return id_;
}

Component::iterator Component::begin() {
  return instances_.begin();
}

Component::iterator Component::end() {
  return instances_.end();
}

Component::const_iterator Component::begin() const {
  return instances_.begin();
}

Component::const_iterator Component::end() const {
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