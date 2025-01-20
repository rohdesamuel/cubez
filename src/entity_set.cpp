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

#include <vector>

#include <cubez/cubez.h>
#include "entity_set.h"
#include "utils.h"

EntitySet::EntitySet() : sparse_(16, -1) {
  dense_.reserve(16);
}

void EntitySet::reserve(size_t size) {
  sparse_.reserve(size);
  dense_.reserve(size);
}

void EntitySet::insert(qbEntity entity) {
  entity = ENTITY_ID(entity);
  if (entity >= sparse_.size()) {
    sparse_.resize(entity + 1, -1);
  }
  sparse_[entity] = dense_.size();
  dense_.push_back(entity);
}

void EntitySet::erase(qbEntity entity) {
  entity = ENTITY_ID(entity);
  dense_[sparse_[entity]] = dense_.back();
  sparse_[dense_.back()] = sparse_[entity];
  dense_.pop_back();
  sparse_[entity] = -1;
}

bool EntitySet::has(qbEntity entity) {
  entity = ENTITY_ID(entity);
  if (entity >= sparse_.size()) {
    return false;
  }
  return sparse_[entity] != -1;
}

void EntitySet::clear() {
  sparse_.resize(0);
  dense_.resize(0);
}

uint64_t EntitySet::size() const {
  return dense_.size();
}

EntitySet::iterator EntitySet::begin() {
  return iterator(this, 0);
}

EntitySet::iterator EntitySet::end() {
  return iterator(this, size());
}

EntitySet::const_iterator EntitySet::begin() const {
  return const_iterator(this, 0);
}

EntitySet::const_iterator EntitySet::end() const {
  return const_iterator(this, size());
}
