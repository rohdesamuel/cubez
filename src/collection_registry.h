#ifndef COLLECTION_REGISTRY__H
#define COLLECTION_REGISTRY__H

#include "cubez.h"

#include <algorithm>
#include <atomic>
#include <unordered_map>

class CollectionRegistry {
 public:
  CollectionRegistry();
  ~CollectionRegistry();

  qbCollection Create(qbId program, qbCollectionAttr attr);
  qbResult Destroy(qbCollection collection);
  qbResult Share(qbCollection collection, qbId program);

 private:
  qbCollection new_collection(qbId id, qbId program, qbCollectionAttr attr);

  std::atomic_long id_;
  std::unordered_map<qbId, std::unordered_map<qbId, qbCollection>> collections_;
};

#endif  // COLLECTION_REGISTRY__H
