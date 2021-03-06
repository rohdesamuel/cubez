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

#ifndef SPARSE_SET__H
#define SPARSE_SET__H

#include <vector>

#include <cubez/cubez.h>

class SparseSet {
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
    iterator(SparseSet* set, size_t index) : set_(set), index_(index) {}

    SparseSet* set_;
    size_t index_;

    friend class SparseSet;
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
    const_iterator(const SparseSet* set, size_t index) : set_(set), index_(index) {}

    const SparseSet* set_;
    size_t index_;

    friend class SparseSet;
  };

  SparseSet() : sparse_(16, -1) {
    dense_.reserve(16);
  }

  void reserve(size_t size) {
    sparse_.reserve(size);
    dense_.reserve(size);
  }

  void insert(uint64_t value) {
    if (value >= sparse_.size()) {
      sparse_.resize(value + 1, -1);
    }
    sparse_[value] = dense_.size();
    dense_.push_back(value);
  }

  void erase(uint64_t value) {
    dense_[sparse_[value]] = dense_.back();
    sparse_[dense_.back()] = sparse_[value];
    dense_.pop_back();
    sparse_[value] = -1;
  }

  void clear() {
    sparse_.resize(0);
    dense_.resize(0);
  }

  bool has(uint64_t value) {
    if (value >= sparse_.size()) {
      return false;
    }
    return sparse_[value] != -1;
  }

  uint64_t size() const {
    return dense_.size();
  }

  iterator begin() {
    return iterator(this, 0);
  }

  iterator end() {
    return iterator(this, size());
  }

  const_iterator begin() const {
    return const_iterator(this, 0);
  }

  const_iterator end() const {
    return const_iterator(this, size());
  }

private:
  std::vector<qbHandle> sparse_;
  std::vector<uint64_t> dense_;
};

#endif  // SPARSE_SET__H
