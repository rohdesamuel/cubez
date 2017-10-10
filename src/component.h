#ifndef COMPONENT__H
#define COMPONENT__H

#include "cubez.h"
#include "byte_vector.h"

#include <map>
#include <vector>

class Component {
public:
  typedef qbId Key;

  typedef std::vector<Key> Keys;
  typedef ByteVector Values;

  // For fast lookup if you have the handle to an entity.
  typedef std::vector<uint64_t> Handles;
  typedef std::vector<qbHandle> FreeHandles;

  // For fast lookup by Entity Id.
  typedef std::map<Key, qbHandle> Index;

  Component(size_t type_size) : values(type_size) {}

  size_t element_size() const {
    return values.element_size();
  }

  qbHandle insert(Key&& key, void* value) {
    qbHandle handle = make_handle();

    index_[key] = handle;

    values.push_back(value);
    keys.push_back(key);
    return handle;
  }

  qbHandle insert(const Key& key, void* value) {
    qbHandle handle = make_handle();

    index_[key] = handle;

    values.push_back(value);
    keys.push_back(key);
    return handle;
  }

  qbHandle insert(Key&& key, const void* value) {
    qbHandle handle = make_handle();

    index_[key] = handle;

    values.push_back(value);
    keys.push_back(key);
    return handle;
  }

  qbHandle insert(const Key& key, const void* value) {
    qbHandle handle = make_handle();

    index_[key] = handle;

    values.push_back(value);
    keys.push_back(key);
    return handle;
  }

  void* operator[](qbHandle handle) {
    return values[handles_[handle]];
  }

  const void* operator[](qbHandle handle) const {
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

  inline void* value(uint64_t index) {
    return values[index];
  }

  inline const void* value(uint64_t index) const {
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

    void* c_from = values[i_from];
    void* c_to = values[i_to];

    release_handle(h_from);

    index_.erase(k_from);
    std::swap(i_from, i_to);
    std::swap(k_from, k_to); keys.pop_back();

    void* tmp = alloca(values.element_size());
    memcpy(tmp, c_from, values.element_size());
    memcpy(c_from, c_to, values.element_size());
    memcpy(c_to, tmp, values.element_size());
    values.pop_back();

    return 0;
  }

  static qbCollection new_collection(const std::string& program,
                                     size_t element_size) {
    qbCollectionAttr attr;
    qb_collectionattr_create(&attr);
    qb_collectionattr_setprogram(attr, program.c_str());
    qb_collectionattr_setimplementation(attr, new Component(element_size));
    qb_collectionattr_setcount(attr, default_count);
    qb_collectionattr_setupdate(attr, default_update);
    qb_collectionattr_setinsert(attr, default_insert);
    qb_collectionattr_setaccessors(attr, default_access_by_offset,
                                   default_access_by_key,
                                   default_access_by_handle);
    qb_collectionattr_setkeyiterator(attr, default_keys, sizeof(Key), 0);
    qb_collectionattr_setvalueiterator(attr, default_values, element_size, 0);
    qb_collectionattr_setcount(attr, default_count);

    qbCollection collection;
    qb_collection_create(&collection, attr);

    qb_collectionattr_destroy(&attr);
    return collection;
  }

  static void* default_access_by_offset(qbCollectionInterface* c, uint64_t offset) {
    Component* t = (Component*)c->collection;
    return t->value(offset);
  }

  static void* default_access_by_handle(qbCollectionInterface* c, qbHandle handle) {
    Component* t = (Component*)c->collection;
    return (*t)[handle];
  }

  static void* default_access_by_key(qbCollectionInterface* c, void* key) {
    Component* t = (Component*)c->collection;
    qbHandle found = t->find(*(const Key*)key);
    if (found >= 0) {
      return (*t)[found];
    }
    return nullptr;
  }

  static void default_update(qbCollectionInterface* c, qbElement* element) {
    Component* t = (Component*)c->collection;
    memmove(t->value(element->offset), element->value, t->element_size());
  }

  static qbHandle default_insert(qbCollectionInterface* c, qbElement* element) {
    Component* t = (Component*)c->collection;
    return t->insert(*(Key*)element->key, element->value);
  }

  static uint64_t default_count(qbCollectionInterface* c) {
    return ((Component*)c->collection)->size();
  }

  static uint8_t* default_keys(qbCollectionInterface* c) {
    return (uint8_t*)((Component*)c->collection)->keys.data();
  }

  static uint8_t* default_values(qbCollectionInterface* c) {
    return (uint8_t*)((Component*)c->collection)->values.data();
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

#endif
