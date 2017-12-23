#include "collection_registry.h"
#include "defs.h"


CollectionRegistry::CollectionRegistry(): id_(0) {}

CollectionRegistry::~CollectionRegistry() {
  for (auto& collections : collections_) {
    for (auto& pair : collections.second) {
      qbCollection collection = pair.second;
      Destroy(collection);
    }
    collections.second.clear();
  }
  collections_.clear();
}

qbCollection CollectionRegistry::Create(qbId program, qbCollectionAttr attr) {
  qbId id = id_++;
  qbCollection ret = new_collection(id, program, attr);
  collections_[program][id] = ret;
  return ret;
}

qbResult CollectionRegistry::Destroy(qbCollection collection) {
  collections_[collection->program_id].erase(collection->id);
  free(collection);
  return QB_OK;
}

qbResult CollectionRegistry::Share(qbCollection collection, qbId program) {
  auto& destination = collections_[program];

  if (destination.find(collection->id) == destination.end()) {
    destination[collection->id] = collection;
    return QB_OK;
  }
  return QB_ERROR_ALREADY_EXISTS;
}

qbCollection CollectionRegistry::new_collection(qbId id, qbId program, qbCollectionAttr attr) {
  qbCollection c = (qbCollection)calloc(1, sizeof(qbCollection_));
  *(qbId*)(&c->id) = id;
  *(qbId*)(&c->program_id) = program;
  c->interface.insert = attr->insert;
  c->interface.by_id = attr->accessor.id;
  c->interface.by_offset = attr->accessor.offset;
  c->interface.remove_by_id = attr->remove_by_id;
  c->interface.remove_by_offset = attr->remove_by_offset;
  c->interface.collection = attr->collection;
  c->count = attr->count;
  c->keys = attr->keys;
  c->values = attr->values;
  return c;
}


