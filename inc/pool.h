/**
* Author: Samuel Rohde (rohde.samuel@gmail.com)
*
* This file is subject to the terms and conditions defined in
* file 'LICENSE.txt', which is part of this source code package.
*/

#ifndef POOL__H
#define POOL__H

#ifdef __COMPILE_AS_WINDOWS__
#define _ENABLE_ATOMIC_ALIGNMENT_FIX
#endif

#include <iostream>

#include <vector>
#include <map>

#include "cubez.h"
#include "common.h"
#include "table.h"

namespace cubez
{

template <typename Value_,
          typename Allocator_ = std::allocator<Value_>>
class Pool {
public:
  typedef qbId Key;
  typedef Value_ Value;

  typedef std::vector<Key> Keys;
  typedef std::vector<Value> Values;

  // For fast lookup if you have the handle to an entity.
  typedef std::vector<uint64_t> Handles;
  typedef std::vector<qbHandle> FreeHandles;

  // For fast lookup by Entity Id.
  typedef std::map<Key, qbHandle> Index;

  Pool() {}

  Pool(std::vector<std::tuple<Key, Value>>&& init_data) {
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

  int64_t remove_by_id(qbId id) {
    qbHandle found = find(id);
    if (found < 0) {
      return -1;
    }

    return remove_by_handle(found);
  }

  int64_t remove_by_offset(qbOffset offset) {
    auto it = std::find(handles_.begin(), handles_.end(), offset);
    if (it == handles_.end()) {
      return -1;
    }
    return remove_by_handle(*it);
  }

  int64_t remove_by_handle(qbHandle handle) {
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

  static qbCollection* new_collection(const std::string& program,
                                      const std::string& name) {
    qbCollectionAttr attr;
    qb_collectionattr_create(&attr);
    qb_collectionattr_setprogram(attr, program.c_str());
    qb_collectionattr_setimplementation(attr, new Pool);
    qb_collectionattr_setcount(attr, default_count);
    qb_collectionattr_setupdate(attr, default_update);
    qb_collectionattr_setinsert(attr, default_insert);
    qb_collectionattr_setaccessors(attr, default_access_by_offset,
                                   default_access_by_id,
                                   default_access_by_handle);
    qb_collectionattr_setremovers(attr, default_remove_by_offset,
                                  default_remove_by_id,
                                  default_remove_by_handle);
    qb_collectionattr_setkeyiterator(attr, default_keys, sizeof(Key), 0);
    qb_collectionattr_setvalueiterator(attr, default_values, sizeof(Value), 0);
    qb_collectionattr_setcount(attr, default_count);

    qbCollection collection;
    qb_collection_create(&collection, attr);

    qb_collectionattr_destroy(&attr);
    return collection;
  }

  static void default_remove_by_offset(qbCollectionInterface* c, uint64_t offset) {
    Pool* t = (Pool*)c->collection;
    t->remove_by_offset(offset);
  }

  static void default_remove_by_handle(qbCollectionInterface* c, qbHandle handle) {
    Pool* t = (Pool*)c->collection;
    t->remove_by_handle(handle);
  }

  static void default_remove_by_id(qbCollectionInterface* c, qbId id) {
    Pool* t = (Pool*)c->collection;
    t->remove_by_id(id);
  }

  static void* default_access_by_offset(qbCollectionInterface* c, uint64_t offset) {
    Pool* t = (Pool*)c->collection;
    return &t->value(offset);
  }

  static void* default_access_by_handle(qbCollectionInterface* c, qbHandle handle) {
    Pool* t = (Pool*)c->collection;
    return &(*t)[handle];
  }

  static void* default_access_by_id(qbCollectionInterface* c, qbId id) {
    Pool* t = (Pool*)c->collection;
    qbHandle found = t->find(id);
    if (found >= 0) {
      return &(*t)[found];
    }
    return nullptr;
  }

  static void default_update(qbCollectionInterface* c, qbElement* el) {
    Pool* t = (Pool*)c->collection;
    t->values[el->offset] = std::move(*(Value*)el->value); 
  }

  static qbHandle default_insert(qbCollectionInterface* c, qbElement* el) {
    Pool* t = (Pool*)c->collection;
    return t->insert(std::move(*(Key*)el->key), std::move(*(Value*)el->value));
  }

  static uint64_t default_count(qbCollectionInterface* c) {
    return ((Pool*)c->collection)->size();
  }

  static uint8_t* default_keys(qbCollectionInterface* c) {
    return (uint8_t*)((Pool*)c->collection)->keys.data();
  };

  static uint8_t* default_values(qbCollectionInterface* c) {
    return (uint8_t*)((Pool*)c->collection)->values.data();
  };

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
