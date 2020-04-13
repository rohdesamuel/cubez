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

#include "game_state.h"
#include "private_universe.h"

GameState::GameState(std::unique_ptr<EntityRegistry> entities,
                     std::unique_ptr<InstanceRegistry> instances,
                     ComponentRegistry* components)
  : entities_(std::move(entities)),
    instances_(std::move(instances)),
    components_(components) {
  destroyed_entities_.resize(10);
  removed_components_.resize(10);
}

GameState::~GameState() {
  for (qbEntity entity : *entities_) {
    EntityDestroyInternal(entity);
  }
}

void GameState::Flush() {
  for (auto& removed_components : removed_components_) {
    while (!removed_components.empty()) {
      auto& removed = removed_components.back();
      EntityRemoveComponentInternal(removed.first, removed.second);
      removed_components.pop_back();
    }
  }

  for (auto& destroyed_entities : destroyed_entities_) {
    while (!destroyed_entities.empty()) {
      auto& destroyed = destroyed_entities.back();
      EntityDestroyInternal(destroyed);
      destroyed_entities.pop_back();
    }
  }
}

qbResult GameState::EntityCreate(qbEntity* entity, const qbEntityAttr_& attr) {
  qbResult result = entities_->CreateEntity(entity, attr);
  instances_->CreateInstancesFor(*entity, attr.component_list, this);
  return result;
}

qbResult GameState::EntityDestroy(qbEntity entity) {
  destroyed_entities_.resize(std::max(destroyed_entities_.size(), (size_t)PrivateUniverse::program_id + 1));
  destroyed_entities_[PrivateUniverse::program_id].push_back(entity);
  return QB_OK;
}

qbResult GameState::EntityDestroyInternal(qbEntity entity) {
  qbResult result = QB_OK;
  if (entities_->Has(entity)) {
    instances_->DestroyInstancesFor(entity, this);
    result = entities_->DestroyEntity(entity);
  }
  return result;
}

qbResult GameState::EntityFind(qbEntity* entity, qbId entity_id) {
  return entities_->Find(entity_id, entity);
}

bool GameState::EntityHasComponent(qbEntity entity, qbComponent component) {
  return (*instances_)[component].Has(entity);
}

qbResult GameState::EntityAddComponent(qbEntity entity, qbComponent component,
                                       void* instance_data) {
  return instances_->CreateInstanceFor(entity, component, instance_data, this);
}

qbResult GameState::EntityRemoveComponent(qbEntity entity, qbComponent component) {
  removed_components_.resize(std::max(removed_components_.size(), (size_t)PrivateUniverse::program_id + 1));
  removed_components_[PrivateUniverse::program_id].emplace_back(entity, component);
  return QB_OK;
}

qbResult GameState::EntityRemoveComponentInternal(qbEntity entity, qbComponent component) {
  return instances_->DestroyInstanceFor(entity, component, this) == 1 ? QB_OK : QB_UNKNOWN;
}

qbResult GameState::ComponentSubscribeToOnCreate(qbSystem system,
                                                 qbComponent component) {
  return components_->SubcsribeToOnCreate(system, component);
}

qbResult GameState::ComponentSubscribeToOnDestroy(qbSystem system,
                                                  qbComponent component) {
  return components_->SubcsribeToOnDestroy(system, component);
}

Component* GameState::ComponentGet(qbComponent component) {
  return &(*instances_)[component];
}

void* GameState::ComponentGetEntityData(qbComponent component, qbEntity entity) {
  return (*instances_)[component][entity];
}

size_t GameState::ComponentGetCount(qbComponent component) {
  return (*instances_)[component].Size();
}