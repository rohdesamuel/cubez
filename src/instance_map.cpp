#include "instance_map.h"

#include <cubez/common.h>
#include <cubez/random.h>
#include <cubez/hash.h>
#include <iostream>

#include "fast_math.h"

InstanceMap::InstanceMap(size_t element_size)
    : element_size_(element_size), sparse_(16, -1),
  dense_values_(element_size) {
}

InstanceMap::InstanceMap(const InstanceMap& other) : dense_values_(other.element_size_) {
  copy(other);
}

InstanceMap::InstanceMap(InstanceMap&& other) : dense_values_(other.element_size_) {
  move(other);
}

InstanceMap& InstanceMap::operator=(const InstanceMap& other) {
  if (this != &other) {
    copy(other);
  }
  return *this;
}

InstanceMap& InstanceMap::operator=(InstanceMap&& other) {
  if (this != &other) {
    move(other);
  }
  return *this;
}

void InstanceMap::reserve(size_t size) {
  sparse_.reserve(size);
  dense_.reserve(size);
  dense_values_.reserve(size);
}

void* InstanceMap::operator[](uint64_t key) {
  key = ENTITY_ID(key);
  if (!has(key)) {
    insert(key, nullptr);
  }
  return dense_values_[sparse_[key]];
}

const void* InstanceMap::operator[](uint64_t key) const {
  key = ENTITY_ID(key);
  return dense_values_[sparse_[key]];
}

void* InstanceMap::at(uint64_t key) {
  return dense_values_[sparse_[ENTITY_ID(key)]];
}

InstanceMap::iterator InstanceMap::begin() {
  return iterator{ this, 0 };
}

InstanceMap::iterator InstanceMap::end() {
  return iterator{ this, size() };
}

InstanceMap::const_iterator InstanceMap::begin() const {
  return const_iterator{ *this, 0 };
}

InstanceMap::const_iterator InstanceMap::end() const {
  return const_iterator{ *this, size() };
}

void InstanceMap::insert(uint64_t key, void* value) {
  key = ENTITY_ID(key);
  if (key >= sparse_.size()) {
    sparse_.resize(key + 1, -1);
  }
  sparse_[key] = dense_.size();
  dense_.push_back(key);
  dense_values_.push_back(value);
}

void InstanceMap::erase(uint64_t key) {
  key = ENTITY_ID(key);

  // Erase the old value.
  memmove(dense_values_[sparse_[key]], dense_values_.back(), element_size_);
  dense_values_.pop_back();

  // Erase from the sparse set.
  dense_[sparse_[key]] = dense_.back();
  sparse_[dense_.back()] = sparse_[key];
  dense_.pop_back();
  sparse_[key] = -1;
}

void InstanceMap::clear() {
  dense_values_.resize(0);
  sparse_.resize(0);
  dense_.resize(0);
}

bool InstanceMap::has(uint64_t key) const {
  key = ENTITY_ID(key);

  return key < sparse_.size() && sparse_[key] != -1;
}

uint64_t InstanceMap::size() const {
  return dense_.size();
}

size_t InstanceMap::capacity() const {
  return std::max(std::max(sparse_.capacity(), dense_values_.capacity()),
    dense_.capacity());
}

size_t InstanceMap::element_size() const {
  return element_size_;
}

void InstanceMap::copy(const InstanceMap& other) {
  dense_values_ = other.dense_values_;
  sparse_ = other.sparse_;
  dense_ = other.dense_;
}

void InstanceMap::move(const InstanceMap& other) {
  dense_values_ = std::move(other.dense_values_);
  sparse_ = std::move(other.sparse_);
  dense_ = std::move(other.dense_);
}