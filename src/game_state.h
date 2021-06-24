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

#ifndef GAME_STATE__H
#define GAME_STATE__H

#include "instance_registry.h"
#include "entity_registry.h"
#include <memory>
#include "sparse_map.h"

// Not thread-safe. Assumed to run in a single program.
class GameState {
public:
  GameState(std::unique_ptr<EntityRegistry> entities,
            std::unique_ptr<InstanceRegistry> instances,
            ComponentRegistry* components);
  ~GameState();

  void Flush();

  // Entity manipulation.
  qbResult EntityCreate(qbEntity* entity, const qbEntityAttr_& attr);
  qbResult EntityDestroy(qbEntity entity);
  qbResult EntityFind(qbEntity* entity, qbId entity_id);
  bool EntityHasComponent(qbEntity entity, qbComponent component);
  void* EntityGetComponent(qbEntity entity, qbComponent component);
  qbResult EntityAddComponent(qbEntity entity, qbComponent component,
                               void* instance_data);
  qbResult EntityRemoveComponent(qbEntity entity, qbComponent component);
  EntityRegistry& Entities();

  // Component manipulation.
  qbResult ComponentSubscribeToOnCreate(qbSystem system, qbComponent component);
  qbResult ComponentSubscribeToOnDestroy(qbSystem system, qbComponent component);
  
  void* ComponentGetEntityData(qbComponent component, qbEntity entity);
  size_t ComponentGetCount(qbComponent component);
  void ComponentLock(qbComponent component, bool is_mutable);
  void ComponentUnlock(qbComponent component, bool is_mutable);
  Component* ComponentGet(qbComponent component);

private:
  
  qbResult EntityRemoveComponentInternal(qbEntity entity, qbComponent component);
  qbResult EntityDestroyInternal(qbEntity entity);

  std::unique_ptr<EntityRegistry> entities_;
  std::unique_ptr<InstanceRegistry> instances_;
  ComponentRegistry* components_;
  SparseSet mutable_components_;

  TypedBlockVector<std::vector<qbEntity>> destroyed_entities_;
  TypedBlockVector<std::vector<std::pair<qbEntity, qbComponent>>> removed_components_;
};

#endif  // GAME_STATE__H