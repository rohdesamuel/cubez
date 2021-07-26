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

#include "entity_registry.h"

#include <cstring>
#include <iostream>

#include "game_state.h"
EntityRegistry::EntityRegistry()
    : id_(0) {
  entities_.reserve(128);
  free_entity_ids_.reserve(10000);
}

void EntityRegistry::Init() {
}

EntityRegistry* EntityRegistry::Clone() {
  EntityRegistry* ret = new EntityRegistry();
  long id = id_;
  ret->id_ = id;
  ret->entities_ = entities_;
  ret->free_entity_ids_ = free_entity_ids_;
  return ret;
}

// Creates an entity. Entity will be available for use next frame. Sends a
// ComponentCreateEvent after all components have been created.
qbResult EntityRegistry::CreateEntity(qbEntity* entity,
                                      const qbEntityAttr_& /** attr */) {
  qbId new_id = AllocEntity();
  entities_.insert(new_id);

  *entity = new_id;

  return qbResult::QB_OK;
}

// Destroys an entity and frees all components. Entity and components will be
// destroyed next frame. Sends a ComponentDestroyEvent before components are
// removed. Frees entity memory after all components have been destroyed.
qbResult EntityRegistry::DestroyEntity(qbEntity entity) {
  INFO("Destroying instances for " << (entity) << "\n");
  if (entities_.has(entity)) {
    entities_.erase(entity);
    free_entity_ids_.push_back(entity);
  }
  return QB_OK;
}

bool EntityRegistry::Has(qbEntity entity) {
  return entities_.has(entity);
}

qbId EntityRegistry::AllocEntity() {
  qbId new_id;
  if (free_entity_ids_.empty()) {
    new_id = id_++;
  } else {
    new_id = free_entity_ids_.back();
    free_entity_ids_.pop_back();
  }

  return new_id;
}
