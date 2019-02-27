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

  ComponentRegistry* Clone();

  qbResult Create(qbComponent* component, qbComponentAttr attr);
  Component* Create(qbComponent component) const;

  qbResult SubcsribeToOnCreate(qbSystem system, qbComponent component);
  qbResult SubcsribeToOnDestroy(qbSystem system, qbComponent component);

  qbResult SendInstanceCreateNotification(qbEntity entity, Component* component, GameState* state) const;
  qbResult SendInstanceDestroyNotification(qbEntity entity, Component* component, GameState* state) const;
private:
  SparseMap<qbComponentAttr_, TypedBlockVector<qbComponentAttr_>> components_defs_;
  std::vector<qbEvent> instance_create_events_;
  std::vector<qbEvent> instance_destroy_events_;
  std::atomic_long id_;
};

#endif  // COMPONENT_REGISTRY__H
