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
  void RegisterSchema(qbComponent component, qbSchema schema);

  qbComponent Find(const std::string& name) const;
  qbSchema FindSchema(const std::string& name) const;
  qbSchema FindSchema(qbComponent component) const;

  qbResult SubcsribeToOnCreate(qbSystem system, qbComponent component);
  qbResult SubcsribeToOnDestroy(qbSystem system, qbComponent component);

  qbResult SendInstanceCreateNotification(qbEntity entity, Component* component, GameState* state) const;
  qbResult SendInstanceDestroyNotification(qbEntity entity, Component* component, GameState* state) const;
private:
  SparseMap<qbComponentAttr_, TypedBlockVector<qbComponentAttr_>> components_defs_;
  SparseMap<qbSchema_, TypedBlockVector<qbSchema_>> components_schemas_;
  std::unordered_map<std::string, qbComponent> components_;
  std::unordered_map<std::string, qbSchema> schemas_;
  std::vector<qbEvent> instance_create_events_;
  std::vector<qbEvent> instance_destroy_events_;
  std::atomic_long id_;
};

#endif  // COMPONENT_REGISTRY__H
