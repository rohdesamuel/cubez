#ifndef COMPONENT_REGISTRY__H
#define COMPONENT_REGISTRY__H

#include "defs.h"
#include "sparse_map.h"

#include <atomic>
#include <unordered_map>

class GameState;
class ComponentRegistry {
 public:
  ComponentRegistry();
  ~ComponentRegistry();

  void Init();
  ComponentRegistry* Clone();

  qbResult Create(qbComponent* component, qbComponentAttr attr);

  Component& operator[](qbId component) {
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

  qbResult SubcsribeToOnCreate(qbSystem system, qbComponent component) const;
  qbResult SubcsribeToOnDestroy(qbSystem system, qbComponent component) const;

 private:
  qbResult SendInstanceCreateNotification(qbEntity entity, Component* component, GameState* state);
  qbResult SendInstanceDestroyNotification(qbEntity entity, Component* component, GameState* state);

  std::vector<qbEvent> instance_create_events_;
  std::vector<qbEvent> instance_destroy_events_;
  SparseMap<Component*, TypedBlockVector<Component*>> components_;

  std::atomic_long id_;
};

#endif  // COMPONENT_REGISTRY__H
