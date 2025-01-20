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

#ifndef ENTITY_SET__H
#define ENTITY_SET__H

#include <vector>

#include <cubez/cubez.h>

class EntitySet {
public:
  class iterator {
  public:
    iterator operator++() {
      ++index_;
      return *this;
    }

    iterator operator+(size_t delta) {
      index_ = std::min(set_->size(), index_ + delta);
      return *this;
    }

    iterator operator+=(size_t delta) {
      return *this + delta;
    }

    bool operator==(const iterator& other) {
      return set_ == other.set_ && index_ == other.index_;
    }

    bool operator!=(const iterator& other) {
      return !(*this == other);
    }

    uint64_t& operator*() {
      return set_->dense_[index_];
    }

  private:
    iterator(EntitySet* set, size_t index) : set_(set), index_(index) {}

    EntitySet* set_;
    size_t index_;

    friend class EntitySet;
  };
  class const_iterator {
  public:
    const_iterator operator++() {
      ++index_;
      return *this;
    }

    const_iterator operator+(size_t delta) {
      index_ = std::min(set_->size(), index_ + delta);
      return *this;
    }

    const_iterator operator+=(size_t delta) {
      return *this + delta;
    }

    bool operator==(const const_iterator& other) {
      return set_ == other.set_ && index_ == other.index_;
    }

    bool operator!=(const const_iterator& other) {
      return !(*this == other);
    }

    uint64_t operator*() {
      return set_->dense_[index_];
    }

  private:
    const_iterator(const EntitySet* set, size_t index) : set_(set), index_(index) {}

    const EntitySet* set_;
    size_t index_;

    friend class EntitySet;
  };

  EntitySet();

  void reserve(size_t size);
  void insert(qbEntity entity);
  bool has(qbEntity entity);
  void erase(qbEntity entity);
  void clear();

  uint64_t size() const;

  iterator begin();
  iterator end();
  const_iterator begin() const;
  const_iterator end() const;

private:
  std::vector<qbHandle> sparse_;
  std::vector<uint64_t> dense_;
};

#endif  // ENTITY_SET__H