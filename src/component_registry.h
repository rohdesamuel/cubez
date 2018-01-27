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
  ~ComponentRegistry();

  void Init();
  ComponentRegistry* Clone();

  qbResult Create(qbComponent* component, qbComponentAttr attr,
    FrameBuffer* frame_buffer);

  Component& operator[](qbId component) {
    return *(Component*)components_[component];
  }

  const Component& operator[](qbId component) const {
    return *(Component*)components_[component];
  }

  qbResult CreateInstancesFor(
      qbEntity entity, const std::vector<qbComponentInstance_>& instances);

  qbResult CreateInstanceFor(qbEntity entity, qbComponent component,
                             void* instance_data);

  int DestroyInstancesFor(qbEntity entity);
  qbResult DestroyInstanceFor(qbEntity entity, qbComponent component);

  qbResult SubcsribeToOnCreate(qbSystem system, qbComponent component);
  qbResult SubcsribeToOnDestroy(qbSystem system, qbComponent component);

 private:
  qbResult SendInstanceCreateNotification(qbEntity entity, qbComponent component);
  qbResult SendInstanceDestroyNotification(qbEntity entity, qbComponent component);

  std::vector<qbEvent> instance_create_events_;
  std::vector<qbEvent> instance_destroy_events_;
  SparseMap<Component*, TypedBlockVector<Component*>> components_;

  std::atomic_long id_;
};

#endif  // COMPONENT_REGISTRY__H
