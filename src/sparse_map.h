/**
* Author: Samuel Rohde (rohde.samuel@gmail.com)
*
* This file is subject to the terms and conditions defined in
* file 'LICENSE.txt', which is part of this source code package.
*/

#ifndef SPARSE_MAP__H
#define SPARSE_MAP__H

#include <vector>
#include "block_vector.h"
#include "byte_vector.h"
#include "common.h"

template<class Value_>
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
    iterator(SparseMap<Value_>* map, size_t index) : map_(map), index_(index) {}

    SparseMap<Value_>* map_;
    size_t index_;

    friend class SparseMap<Value_>;
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

    bool operator==(const iterator& other) const {
      return map_ == other.map_ && index_ == other.index_;
    }

    bool operator!=(const iterator& other) const {
      return !(*this == other);
    }

    std::pair<qbId, Value_*> operator*() {
      return{ map_->dense_[index_], map_->dense_values_[index_] };
    }

  private:
    const_iterator(const SparseMap<Value_>* map, size_t index) : map_(map), index_(index) {}

    const SparseMap<Value_>* map_;
    size_t index_;

    friend class SparseMap<Value_>;
  };

  SparseMap() : sparse_(16, -1) {}

  SparseMap(const SparseMap<Value_>& other) {
    copy(other);
  }

  SparseMap(SparseMap<Value_>&& other) {
    move(other);
  }

  SparseMap& operator=(const SparseMap<Value_>& other) {
    if (this != &other) {
      copy(other);
    }
    return *this;
  }

  SparseMap& operator=(SparseMap<Value_>&& other) {
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

private:
  void copy(const SparseMap<Value_>& other) {
    dense_values_ = other.dense_values_;
    sparse_ = other.sparse_;
    dense_ = other.dense_;
  }

  void move(const SparseMap<Value_>& other) {
    dense_values_ = std::move(other.dense_values_);
    sparse_ = std::move(other.sparse_);
    dense_ = std::move(other.dense_);
  }

  std::vector<Value> dense_values_;
  std::vector<qbId> sparse_;
  std::vector<uint64_t> dense_;

  friend class iterator;
};

template<>
class SparseMap<void> {
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
    iterator(SparseMap<void>* map, size_t index) : map_(map), index_(index) {}

    SparseMap<void>* map_;
    size_t index_;

    friend class SparseMap<void>;
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

    bool operator==(const iterator& other) const {
      return map_ == other.map_ && index_ == other.index_;
    }

    bool operator!=(const iterator& other) const {
      return !(*this == other);
    }

    std::pair<qbId, const void*> operator*() {
      return{ map_->dense_[index_], map_->dense_values_[index_] };
    }

  private:
    const_iterator(const SparseMap<void>* map, size_t index) : map_(map), index_(index) {}

    const SparseMap<void>* map_;
    size_t index_;

    friend class SparseMap<void>;
  };

  SparseMap(size_t element_size)
    : element_size_(element_size), sparse_(16, -1), 
      dense_values_(element_size) {}

  SparseMap(const SparseMap<void>& other) : dense_values_(other.element_size_) {
    copy(other);
  }

  SparseMap(SparseMap<void>&& other) : dense_values_(other.element_size_) {
    move(other);
  }

  SparseMap& operator=(const SparseMap<void>& other) {
    if (this != &other) {
      copy(other);
    }
    return *this;
  }

  SparseMap& operator=(SparseMap<void>&& other) {
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
    return const_iterator{ this, 0 };
  }

  const_iterator end() const {
    return const_iterator{ this, size() };
  }

  void insert(uint64_t key, void* value) {
    if (key >= sparse_.size()) {
      sparse_.resize(key + 1, -1);
    }
    sparse_[key] = dense_.size();
    dense_.push_back(key);
    dense_values_.push_back(std::move(value));
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

  size_t element_size() const {
    return element_size_;
  }

private:
  void copy(const SparseMap<void>& other) {
    dense_values_ = other.dense_values_;
    sparse_ = other.sparse_;
    dense_ = other.dense_;
  }

  void move(const SparseMap<void>& other) {
    dense_values_ = std::move(other.dense_values_);
    sparse_ = std::move(other.sparse_);
    dense_ = std::move(other.dense_);
  }

  size_t element_size_;
  std::vector<qbId> sparse_;
  BlockVector dense_values_;
  std::vector<uint64_t> dense_;
};

#endif  // SPARSE_MAP__H
