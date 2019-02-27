#ifndef INSTANCE_REGISTRY__H
#define INSTANCE_REGISTRY__H

#include "defs.h"
#include "sparse_map.h"
#include "component_registry.h"

#include <atomic>
#include <unordered_map>

class GameState;
class InstanceRegistry {
public:
  InstanceRegistry(const ComponentRegistry& component_registry);
  ~InstanceRegistry();

  InstanceRegistry* Clone();

  Component& operator[](qbId component) {
    Create(component);
    return *(Component*)components_[component];
  }

  const Component& operator[](qbId component) const {
    return *(Component*)components_[component];
  }

  qbResult CreateInstancesFor(
    qbEntity entity, const std::vector<qbComponentInstance_>& instances,
    GameState* state);

  qbResult CreateInstanceFor(qbEntity entity, qbComponent component,
                             void* instance_data, GameState* state);

  int DestroyInstancesFor(qbEntity entity, GameState* state);
  int DestroyInstanceFor(qbEntity entity, qbComponent component,
                         GameState* state);

  qbResult SendInstanceCreateNotification(qbEntity entity, Component* component, GameState* state) const;
  qbResult SendInstanceDestroyNotification(qbEntity entity, Component* component, GameState* state) const;

private:
  void Create(qbComponent component);

  const ComponentRegistry& component_registry_;
  SparseMap<Component*, TypedBlockVector<Component*>> components_;
};

#endif  // INSTANCE_REGISTRY__H
