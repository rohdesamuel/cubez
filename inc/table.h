/**
* Author: Samuel Rohde (rohde.samuel@gmail.com)
*
* This file is subject to the terms and conditions defined in
* file 'LICENSE.txt', which is part of this source code package.
*/

#ifndef TABLE__H
#define TABLE__H

#ifdef __COMPILE_AS_WINDOWS__
#define _ENABLE_ATOMIC_ALIGNMENT_FIX
#endif

#include <iostream>

#include <vector>
#include <map>

#include "cubez.h"
#include "common.h"

namespace cubez
{

template <typename Key_, typename Value_,
          typename Allocator_ = std::allocator<Value_>>
class Table {
public:
  typedef Key_ Key;
  typedef Value_ Value;

  typedef std::vector<Key> Keys;
  typedef std::vector<Value> Values;

  // For fast lookup if you have the handle to an entity.
  typedef std::vector<uint64_t> Handles;
  typedef std::vector<qbHandle> FreeHandles;

  // For fast lookup by Entity Id.
  typedef std::map<Key, qbHandle> Index;

  Table() {}

  Table(std::vector<std::tuple<Key, Value>>&& init_data) {
    for (auto& t : init_data) {
      insert(std::move(std::get<0>(t)), std::move(std::get<1>(t)));
    }
  }

  qbHandle insert(Key&& key, Value&& value) {
    qbHandle handle = make_handle();

    index_[key] = handle;

    values.push_back(std::move(value));
    keys.push_back(key);
    return handle;
  }

  qbHandle insert(Key&& key, const Value& value) {
    qbHandle handle = make_handle();

    index_[key] = handle;

    values.push_back(value);
    keys.push_back(key);
    return handle;
  }

  qbHandle insert(const Key& key, const Value& value) {
    qbHandle handle = make_handle();

    index_[key] = handle;

    values.push_back(value);
    keys.push_back(key);
    return handle;
  }

  qbHandle insert(const Key& key, Value&& value) {
    qbHandle handle = make_handle();

    index_[key] = handle;

    values.push_back(std::move(value));
    keys.push_back(key);
    return handle;
  }

  Value& operator[](qbHandle handle) {
    return values[handles_[handle]];
  }

  const Value& operator[](qbHandle handle) const {
    return values[handles_[handle]];
  }

  qbHandle find(Key&& key) const {
    typename Index::const_iterator it = index_.find(key);
    if (it != index_.end()) {
      return it->second;
    }
    return -1;
  }

  qbHandle find(const Key& key) const {
    typename Index::const_iterator it = index_.find(key);
    if (it != index_.end()) {
      return it->second;
    }
    return -1;
  }

  inline Key& key(uint64_t index) {
    return keys[index];
  }

  inline const Key& key(uint64_t index) const {
    return keys[index];
  }

  inline Value& value(uint64_t index) {
    return values[index];
  }

  inline const Value& value(uint64_t index) const {
    return values[index];
  }

  uint64_t size() const {
    return values.size();
  }

  int64_t remove(qbHandle handle) {
    qbHandle h_from = handle;
    uint64_t& i_from = handles_[h_from];
    Key& k_from = keys[i_from];

    Key& k_to = keys.back();
    qbHandle h_to = index_[k_to];
    uint64_t& i_to = handles_[h_to];

    Value& c_from = values[i_from];
    Value& c_to = values[i_to];

    release_handle(h_from);

    index_.erase(k_from);
    std::swap(i_from, i_to);
    std::swap(k_from, k_to); keys.pop_back();
    std::swap(c_from, c_to); values.pop_back();

    return 0;
  }


  Keys keys;
  Values values;

private:
  qbHandle make_handle() {
    // Use reusable stale handles first.
    if (!free_handles_.empty()) {
      qbHandle h = free_handles_.back();
      free_handles_.pop_back();
      return h;
    }

    // Otherwise make a new handle.
    handles_.push_back(handles_.size());
    return handles_.back();
  }

  void release_handle(qbHandle h) {
    free_handles_.push_back(h);
  }

  Handles handles_;
  FreeHandles free_handles_;
  Index index_;
};

}  // namespace cubez

#endif
