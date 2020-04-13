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

#ifndef SPARSE_MAP__H
#define SPARSE_MAP__H

#include <cubez/cubez.h>
#include <vector>
#include "block_vector.h"
#include "byte_vector.h"

template<class Value_, class Container_>
class SparseMap {
public:
  typedef uint64_t Key;
  typedef Value_ Value;

  class iterator {
  public:
    iterator operator++() {
      ++index_;
      return *this;
    }

    iterator operator+(size_t delta) {
      index_ = std::min(map_->size(), index_ + delta);
      return *this;
    }

    iterator operator+=(size_t delta) {
      return *this + delta;
    }

    bool operator==(const iterator& other) {
      return map_ == other.map_ && index_ == other.index_;
    }

    bool operator!=(const iterator& other) {
      return !(*this == other);
    }

    std::pair<qbId, Value_&> operator*() {
      return{ map_->dense_[index_], map_->dense_values_[index_] };
    }

  private:
    iterator(SparseMap* map, size_t index) : map_(map), index_(index) {}

    SparseMap* map_;
    size_t index_;

    friend class SparseMap;
  };

  class const_iterator {
  public:
    const_iterator operator++() {
      ++index_;
      return *this;
    }

    const_iterator operator+(size_t delta) {
      index_ = std::min(map_->size(), index_ + delta);
      return *this;
    }

    const_iterator operator+=(size_t delta) {
      return *this + delta;
    }

    bool operator==(const const_iterator& other) {
      return map_ == other.map_ && index_ == other.index_;
    }

    bool operator!=(const const_iterator& other) {
      return !(*this == other);
    }

    std::pair<qbId, const Value_&> operator*() {
      return std::make_pair(map_->dense_.at(index_), map_->dense_values_.at(index_));
    }

  private:
    const_iterator(const SparseMap* map, size_t index) : map_(map), index_(index) {}

    const SparseMap* map_;
    size_t index_;

    friend class SparseMap;
  };

  SparseMap() : sparse_(16, -1) {}

  SparseMap(const SparseMap& other) {
    copy(other);
  }

  SparseMap(SparseMap&& other) {
    move(other);
  }

  SparseMap& operator=(const SparseMap& other) {
    if (this != &other) {
      copy(other);
    }
    return *this;
  }

  SparseMap& operator=(SparseMap&& other) {
    if (this != &other) {
      move(other);
    }
    return *this;
  }

  void reserve(size_t size) {
    sparse_.reserve(size);
    dense_.reserve(size);
    dense_values_.reserve(size);
  }

  Value& operator[](uint64_t key) {
    if (!has(key)) {
      insert(key, Value{});
    }
    return dense_values_[sparse_[key]];
  }

  const Value& operator[](uint64_t key) const {
    return dense_values_[sparse_[key]];
  }

  iterator begin() {
    return iterator{ this, 0 };
  }

  iterator end() {
    return iterator{ this, size() };
  }

  const_iterator begin() const {
    return const_iterator{ this, 0 };
  }

  const_iterator end() const {
    return const_iterator{ this, size() };
  }

  void insert(uint64_t key, const Value& value) {
    if (key >= sparse_.size()) {
      sparse_.resize(key + 1, -1);
    }
    sparse_[key] = dense_.size();
    dense_.push_back(key);
    dense_values_.push_back(value);
  }

  void insert(uint64_t key, Value&& value) {
    if (key >= sparse_.size()) {
      sparse_.resize(key + 1, -1);
    }
    sparse_[key] = dense_.size();
    dense_.push_back(key);
    dense_values_.push_back(std::move(value));
  }

  void erase(uint64_t key) {
    // Erase the old value.
    dense_values_[sparse_[key]] = std::move(dense_values_.back());
    dense_values_.pop_back();

    // Erase from the sparse set.
    dense_[sparse_[key]] = dense_.back();
    sparse_[dense_.back()] = sparse_[key];
    dense_.pop_back();
    sparse_[key] = -1;
  }

  void clear() {
    dense_values_.resize(0);
    sparse_.resize(0);
    dense_.resize(0);
  }

  bool has(uint64_t key) {
    if (key >= sparse_.size()) {
      return false;
    }
    return sparse_[key] != -1;
  }

  uint64_t size() const {
    return dense_.size();
  }

  size_t capacity() const {
    return std::max(std::max(sparse_.capacity(), dense_values_.capacity()), 
                    dense_.capacity());
  }

private:
  void copy(const SparseMap& other) {
    dense_values_ = other.dense_values_;
    sparse_ = other.sparse_;
    dense_ = other.dense_;
  }

  void move(const SparseMap& other) {
    dense_values_ = std::move(other.dense_values_);
    sparse_ = std::move(other.sparse_);
    dense_ = std::move(other.dense_);
  }

  Container_ dense_values_;
  std::vector<qbId> sparse_;
  std::vector<uint64_t> dense_;
};

template<class Container_>
class SparseMap<void, Container_> {
public:
  typedef uint64_t Key;

  class iterator {
  public:
    iterator operator++() {
      ++index_;
      return *this;
    }

    iterator operator+(size_t delta) {
      index_ = std::min(map_->size(), index_ + delta);
      return *this;
    }

    iterator operator+=(size_t delta) {
      return *this + delta;
    }

    bool operator==(const iterator& other) const {
      return map_ == other.map_ && index_ == other.index_;
    }

    bool operator!=(const iterator& other) const {
      return !(*this == other);
    }

    std::pair<qbId, void*> operator*() {
      return{ map_->dense_[index_], map_->dense_values_[index_] };
    }

  private:
    iterator(SparseMap* map, size_t index) : map_(map), index_(index) {}

    SparseMap* map_;
    size_t index_;

    friend class SparseMap;
  };

  class const_iterator {
  public:
    const_iterator operator++() {
      ++index_;
      return *this;
    }

    const_iterator operator+(size_t delta) {
      index_ = std::min(map_->size(), index_ + delta);
      return *this;
    }

    const_iterator operator+=(size_t delta) {
      return *this + delta;
    }

    bool operator==(const const_iterator& other) const {
      return &map_ == &other.map_ && index_ == other.index_;
    }

    bool operator!=(const const_iterator& other) const {
      return !(*this == other);
    }

    std::pair<qbId, const void*> operator*() const {
      return{ map_.dense_[index_], map_.dense_values_[index_] };
    }

  private:
    const_iterator(const SparseMap& map, size_t index) : map_(map), index_(index) {}

    const SparseMap& map_;
    size_t index_;

    friend class SparseMap;
  };

  SparseMap(size_t element_size)
    : element_size_(element_size), sparse_(16, -1),
    dense_values_(element_size) {}

  SparseMap(const SparseMap& other) : dense_values_(other.element_size_) {
    copy(other);
  }

  SparseMap(SparseMap&& other) : dense_values_(other.element_size_) {
    move(other);
  }

  SparseMap& operator=(const SparseMap& other) {
    if (this != &other) {
      copy(other);
    }
    return *this;
  }

  SparseMap& operator=(SparseMap&& other) {
    if (this != &other) {
      move(other);
    }
    return *this;
  }

  void reserve(size_t size) {
    sparse_.reserve(size);
    dense_.reserve(size);
    dense_values_.reserve(size);
  }

  void* operator[](uint64_t key) {
    if (!has(key)) {
      insert(key, nullptr);
    }
    return dense_values_[sparse_[key]];
  }

  const void* operator[](uint64_t key) const {
    return dense_values_[sparse_[key]];
  }

  iterator begin() {
    return iterator{ this, 0 };
  }

  iterator end() {
    return iterator{ this, size() };
  }

  const_iterator begin() const {
    return const_iterator{ *this, 0 };
  }

  const_iterator end() const {
    return const_iterator{ *this, size() };
  }

  void insert(uint64_t key, void* value) {
    if (key >= sparse_.size()) {
      sparse_.resize(key + 1, -1);
    }
    sparse_[key] = dense_.size();
    dense_.push_back(key);
    dense_values_.push_back(value);
  }

  void erase(uint64_t key) {
    // Erase the old value.
    memmove(dense_values_[sparse_[key]], dense_values_.back(), element_size_);
    dense_values_.pop_back();

    // Erase from the sparse set.
    dense_[sparse_[key]] = dense_.back();
    sparse_[dense_.back()] = sparse_[key];
    dense_.pop_back();
    sparse_[key] = -1;
  }

  void clear() {
    dense_values_.resize(0);
    sparse_.resize(0);
    dense_.resize(0);
  }

  bool has(uint64_t key) const {
    if (key >= sparse_.size()) {
      return false;
    }
    return sparse_[key] != -1;
  }

  uint64_t size() const {
    return dense_.size();
  }

  size_t capacity() const {
    return std::max(std::max(sparse_.capacity(), dense_values_.capacity()),
                    dense_.capacity());
  }

  size_t element_size() const {
    return element_size_;
  }

private:
  void copy(const SparseMap& other) {
    dense_values_ = other.dense_values_;
    sparse_ = other.sparse_;
    dense_ = other.dense_;
  }

  void move(const SparseMap& other) {
    dense_values_ = std::move(other.dense_values_);
    sparse_ = std::move(other.sparse_);
    dense_ = std::move(other.dense_);
  }

  size_t element_size_;
  std::vector<qbId> sparse_;
  Container_ dense_values_;
  std::vector<uint64_t> dense_;
};

#endif  // SPARSE_MAP__H
