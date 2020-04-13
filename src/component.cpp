/**
* Author: Samuel Rohde (rohde.samuel@cubez.io)
*
* Copyright 2020 Samuel Rohde
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "apex_memmove.h"
#include "component.h"
#include "defs.h"

#include <omp.h>

Component::Component(qbId id, size_t instance_size, bool is_shared, qbComponentType type)
    : id_(id), instances_(instance_size), is_shared_(is_shared), type_(type) {}

Component* Component::Clone() {
  Component* ret = new Component(id_, instances_.element_size(), is_shared_, type_);
  ret->instances_ = instances_;
  return ret;
}

void Component::Merge(const Component& other) {
  size_t size = instances_.element_size();
  for (const auto& pair : other) {
    const void* src = pair.second;
    void* dst = instances_[pair.first];
    if (memcmp(src, dst, size) != 0) {
      memcpy(dst, src, size);
    }
  }
}

qbResult Component::Create(qbId entity, void* value) {
  instances_.insert(entity, value);
  return QB_OK;
}

qbResult Component::Destroy(qbId entity) {
  if (instances_.has(entity)) {
    if (type_ == qbComponentType::QB_COMPONENT_TYPE_COMPOSITE) {
      qbEntity* entities = (qbEntity*)instances_[entity];
      for (size_t i = 0; i < instances_.size() / sizeof(qbEntity); ++i) {
        qb_entity_destroy(entities[i]);
      }
    } else if (type_ == qbComponentType::QB_COMPONENT_TYPE_POINTER) {
      free(*(void**)instances_[entity]);
    }
    instances_.erase(entity);
  }
  return QB_OK;
}

void* Component::operator[](qbId entity) {
  return instances_[entity];
}

const void* Component::operator[](qbId entity) const {
  return instances_[entity];
}

const void* Component::at(qbId entity) const {
  return (*this)[entity];
}

bool Component::Has(qbId entity) const {
  return instances_.has(entity);
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

void Component::Lock(bool is_mutable) {
  if (!is_shared_) {
    return;
  }

  if (is_mutable) {
    mu_.lock();
  } else {
    mu_.lock_shared();
  }
}

void Component::Unlock(bool is_mutable) {
  if (!is_shared_) {
    return;
  }

  if (is_mutable) {
    mu_.unlock();
  } else {
    mu_.unlock_shared();
  }
}