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
                                      const std::string& collection) {

    qbCollection* c = qb_alloc_collection(program.data(), collection.data());
    c->collection = new Table();

    c->accessor.offset = default_access_by_offset;
    c->accessor.handle = default_access_by_handle;
    c->accessor.key = default_access_by_key;

    c->copy = default_copy;
    c->mutate = default_mutate;
    c->count = default_count;

    c->keys.data = default_keys;
    c->keys.stride = sizeof(Key);
    c->keys.offset = 0;

    c->values.data = default_values;
    c->values.stride = sizeof(Value);
    c->values.offset = 0;

    return c;
  }

  static void* default_access_by_offset(qbCollection* c, uint64_t offset) {
    Table* t = (Table*)c->collection;
    return &t->value(offset);
  }

  static void* default_access_by_handle(qbCollection* c, qbHandle handle) {
    Table* t = (Table*)c->collection;
    return &(*t)[handle];
  }

  static void* default_access_by_key(qbCollection* c, void* key) {
    Table* t = (Table*)c->collection;
    return &(*t)[t->find(*(const Key_*)key)];
  }

  static void default_copy(const uint8_t* /* key */,
                           const uint8_t* value,
                           uint64_t offset,
                           qbFrame* f) {
    qbMutation* mutation = &f->mutation;
    mutation->mutate_by = qbMutateBy::UPDATE;
    Element* el = (Element*)(mutation->element);
    el->offset = offset;
    new (&el->value) Value(*(Value*)(value) );
  }

  static void default_mutate(qbCollection* c,
                             const qbMutation* m) {
      Table* t = (Table*)c->collection;
      Element* el = (Element*)(m->element);
      if (m->mutate_by == qbMutateBy::UPDATE) {
        t->values[el->offset] = std::move(el->value); 
      } else if (m->mutate_by == qbMutateBy::INSERT) {
        t->insert(std::move(el->key), std::move(el->value));
      }
  }

  static uint64_t default_count(qbCollection* c) {
    return ((Table*)c->collection)->size();
  }

  static uint8_t* default_keys(qbCollection* c) {
    return (uint8_t*)((Table*)c->collection)->keys.data();
  };

  static uint8_t* default_values(qbCollection* c) {
    return (uint8_t*)((Table*)c->collection)->values.data();
  };

  Keys keys;
  Values values;

private:
  qbHandle make_handle() {
    if (free_handles_.size()) {
      qbHandle h = free_handles_.back();
      free_handles_.pop_back();
      return h;
    }
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
