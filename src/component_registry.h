#ifndef COMPONENT_REGISTRY__H
#define COMPONENT_REGISTRY__H

#include "defs.h"
#include "sparse_map.h"

#include <atomic>
#include <unordered_map>

class ComponentRegistry {
 public:
  ComponentRegistry();

  typedef SparseMap<qbComponent_*>::iterator iterator;

  iterator begin() {
    return components_.begin();
  }

  iterator end() {
    return components_.end();
  }

  qbComponent_& operator[](qbId component) {
    return *components_[component];
  }

  const qbComponent_& operator[](qbId component) const {
    return *components_[component];
  }

  qbResult CreateComponent(qbComponent* component, qbComponentAttr attr);

  static qbId CreateComponentInstance(
      qbComponent component, qbId entity_id, const void* value);

  static void DestroyComponentInstance(qbComponent component, qbId handle);

  static qbResult SendComponentCreateEvent(qbComponent component,
                                           qbId entity,
                                           void* instance_data);

  static qbResult SendComponentDestroyEvent(qbComponent component,
                                            qbId entity,
                                            void* instance_data);

 private:
  SparseMap<qbComponent> components_;
  std::atomic_long id_;
};

#endif  // COMPONENT_REGISTRY__H
