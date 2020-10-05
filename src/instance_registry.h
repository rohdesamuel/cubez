/**
* Author: Samuel Rohde (rohde.samuel@cubez.io)
*
* Copyright 2020 Samuel Rohde
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

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

  bool InstanceHas(qbEntity entity, qbComponent component);
  void* InstanceData(qbEntity entity, qbComponent component);
  size_t InstanceCount(qbComponent component);

  void Lock(qbComponent, bool is_mutable);
  void Unlock(qbComponent, bool is_mutable);

  Component& operator[](qbId component) {
    Create(component);
    return *(Component*)components_[component];
  }

  const Component& operator[](qbId component) const {
    return *(Component*)components_[component];
  }

private:

  void Create(qbComponent component);

  const ComponentRegistry& component_registry_;
  SparseMap<Component*, TypedBlockVector<Component*>> components_;
};

#endif  // INSTANCE_REGISTRY__H
