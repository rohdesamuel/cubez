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

template<typename Key_, typename Value_>
struct BaseElement {
  qbIndexedBy indexed_by;
  union {
    qbOffset offset;
    qbHandle handle;
    Key_ key;
  };
  Value_ value;
};

template <typename Key_, typename Value_,
          typename Allocator_ = std::allocator<Value_>>
class Table {
public:
  typedef Key_ Key;
  typedef Value_ Value;

  typedef BaseElement<Key, Value> Element;

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

  static qbCollection* new_collection(const std::string& program,
                                      const std::string& name) {
    qbCollectionAttr attr;
    qb_collectionattr_create(&attr);
    qb_collectionattr_setprogram(&attr, program.c_str());
    qb_collectionattr_setimplementation(&attr, new Table);
    qb_collectionattr_setcount(&attr, default_count);
    qb_collectionattr_setupdate(&attr, default_update);
    qb_collectionattr_setinsert(&attr, default_insert);
    qb_collectionattr_setaccessors(&attr, default_access_by_offset,
                                   default_access_by_key,
                                   default_access_by_handle);
    qb_collectionattr_setkeyiterator(&attr, default_keys, sizeof(Key), 0);
    qb_collectionattr_setvalueiterator(&attr, default_values, sizeof(Value), 0);
    qb_collectionattr_setcount(&attr, default_count);

    qbCollection collection;
    qb_collection_create(&collection, name.c_str(), attr);

    qb_collectionattr_destroy(&attr);
    return collection;
  }

  static void* default_access_by_offset(qbCollectionInterface* c, uint64_t offset) {
    Table* t = (Table*)c->collection;
    return &t->value(offset);
  }

  static void* default_access_by_handle(qbCollectionInterface* c, qbHandle handle) {
    Table* t = (Table*)c->collection;
    return &(*t)[handle];
  }

  static void* default_access_by_key(qbCollectionInterface* c, void* key) {
    Table* t = (Table*)c->collection;
    qbHandle found = t->find(*(const Key*)key);
    if (found >= 0) {
      return &(*t)[found];
    }
    return nullptr;
  }

  static void default_update(qbCollectionInterface* c, qbElement* el) {
    Table* t = (Table*)c->collection;
    t->values[el->offset] = std::move(*(Value*)el->value); 
  }

  static qbHandle default_insert(qbCollectionInterface* c, qbElement* el) {
    Table* t = (Table*)c->collection;
    return t->insert(std::move(*(Key*)el->key), std::move(*(Value*)el->value));
  }

  static uint64_t default_count(qbCollectionInterface* c) {
    return ((Table*)c->collection)->size();
  }

  static uint8_t* default_keys(qbCollectionInterface* c) {
    return (uint8_t*)((Table*)c->collection)->keys.data();
  };

  static uint8_t* default_values(qbCollectionInterface* c) {
    return (uint8_t*)((Table*)c->collection)->values.data();
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

template <typename Table_>
class View {
public:
  typedef Table_ Table;
  typedef BaseElement<typename Table::Key, const typename Table::Value&> Element;

  View(Table* table) : table_(table) {}

  inline const typename Table::Value& operator[](qbHandle handle) const {
    return table_->operator[](handle);
  }

  inline qbHandle find(typename Table::Key&& key) const {
    return table_->find(std::move(key));
  }

  inline qbHandle find(const typename Table::Key& key) const {
    return table_->find(key);
  }

  inline const typename Table::Key& key(uint64_t index) const {
    return table_->keys[index];
  }

  inline const typename Table::Value& value(uint64_t index) const {
    return table_->values[index];
  }

  inline uint64_t size() const {
    return table_->values.size();
  }
private:
  Table* table_;
};

}  // namespace cubez

#endif
