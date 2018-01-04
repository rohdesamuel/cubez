#ifndef COMPONENT_REGISTRY__H
#define COMPONENT_REGISTRY__H

#include "defs.h"
#include "sparse_map.h"

#include <atomic>
#include <unordered_map>

class ComponentRegistry {
 public:
  ComponentRegistry();

  void Init();

  struct InstanceCreateEvent {
    qbId entity;
    qbComponent component;
    void* instance_data;
  };

  struct InstanceDestroyEvent {
    qbId entity;
    qbComponent component;
  };

  Component& operator[](qbId component) {
    return components_[component]->instances;
  }

  const Component& operator[](qbId component) const {
    return components_[component]->instances;
  }

  qbResult ComponentCreate(qbComponent* component, qbComponentAttr attr);

  qbResult CreateInstancesFor(
      qbEntity entity, const std::vector<qbComponentInstance_>& instances);

  qbResult CreateInstanceFor(qbEntity entity, qbComponent component,
                             void* instance_data);

  int DestroyInstancesFor(qbEntity entity);
  qbResult DestroyInstanceFor(qbEntity entity, qbComponent component);

 private:
  static void InstanceCreateHandler(qbFrame* frame);
  static void InstanceDestroyHandler(qbFrame* frame);

  SparseMap<qbComponent> components_;
  std::atomic_long id_;

  qbEvent instance_create_event_;
  qbEvent instance_destroy_event_;

  qbSystem instance_create_system_;
  qbSystem instance_destroy_system_;
};

#endif  // COMPONENT_REGISTRY__H
