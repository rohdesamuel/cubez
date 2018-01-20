#ifndef COMPONENT_REGISTRY__H
#define COMPONENT_REGISTRY__H

#include "defs.h"
#include "sparse_map.h"
#include "frame_buffer.h"

#include <atomic>
#include <unordered_map>

class ComponentRegistry {
 public:
  ComponentRegistry();

  void Init();

  Component& operator[](qbId component) {
    return components_[component]->instances;
  }

  const Component& operator[](qbId component) const {
    return components_[component]->instances;
  }

  qbResult ComponentCreate(qbComponent* component, qbComponentAttr attr,
                           FrameBuffer* frame_buffer);

  qbResult CreateInstancesFor(
      qbEntity entity, const std::vector<qbComponentInstance_>& instances);

  qbResult CreateInstanceFor(qbEntity entity, qbComponent component,
                             void* instance_data);

  int DestroyInstancesFor(qbEntity entity);
  qbResult DestroyInstanceFor(qbEntity entity, qbComponent component);

 private:
  SparseMap<qbComponent> components_;
  std::atomic_long id_;
};

#endif  // COMPONENT_REGISTRY__H
