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

#include "instance_registry.h"

#include "component.h"
#include "game_state.h"
#include "utils.h"
#include "entity_table.h"

InstanceRegistry::InstanceRegistry(const ComponentRegistry& component_registry, TableRegistry& table_registry) :
  component_registry_(component_registry),
  table_registry_(table_registry) {}

InstanceRegistry::~InstanceRegistry() {
  for (auto c_pair : components_) {
    delete c_pair.second;
  }
}

InstanceRegistry* InstanceRegistry::Clone() {
  InstanceRegistry* ret = new InstanceRegistry(component_registry_, table_registry_);
  for (auto c_pair : components_) {
    ret->components_[c_pair.first] = c_pair.second->Clone();
  }
  return ret;
}

void InstanceRegistry::Create(qbComponent component) {
  if (components_.has(component)) {
    return;
  }
  components_[component] = component_registry_.Create(component);
}

qbResult InstanceRegistry::CreateInstancesFor(
  qbEntity entity, const std::vector<qbComponentData_>& instances,
  GameState* state) {
  qbEntity entity_id = ENTITY_ID(entity);

  for (auto& instance : instances) {
    Create(instance.component);
    Component* component = components_[instance.component];
    component->Create(entity_id, instance.data);
  }

  for (auto& instance : instances) {
    Component* component = components_[instance.component];
    SendInstanceCreateNotification(entity, component, state);
  }

  return QB_OK;
}

qbResult InstanceRegistry::CreateInstancesFor(
  qbEntity entity, size_t count, const qbComponentData_ data[],
  GameState* state) {
  qbEntity entity_id = ENTITY_ID(entity);

  for (size_t i = 0; i < count; ++i) {
    const qbComponentData_* instance = data + i;
    const qbComponent qb_component = instance->component;
    void* data = instance->data;

    Create(qb_component);
    Component* component = components_[qb_component];
    component->Create(entity_id, data);
  }

  for (size_t i = 0; i < count; ++i) {
    Component* component = components_[data[i].component];
    SendInstanceCreateNotification(entity, component, state);
  }

  return QB_OK;
}

qbResult InstanceRegistry::CreateInstanceFor(qbEntity entity,
                                              qbComponent component,
                                              void* instance_data,
                                              GameState* state) {
  Create(component);
  qbEntity entity_id = ENTITY_ID(entity);
  Component* c = components_[component];
  c->Create(entity_id, instance_data);
  SendInstanceCreateNotification(entity, c, state);
  return QB_OK;
}

int InstanceRegistry::DestroyInstancesFor(qbEntity entity, GameState* state) {
  qbEntity entity_id = ENTITY_ID(entity);
  qbId table_id = ENTITY_TABLE_ID(entity);

  if (table_id) {
    EntityTable* table = table_registry_.Find(table_id);
    table->destroy_entity(entity);
  }

  int destroyed_instances = 0;
  for (auto component_pair : components_) {
    Component* component = component_pair.second;
    if (component->Has(entity_id)) {
      SendInstanceDestroyNotification(entity, component, state);
    }
  }

  for (auto component_pair : components_) {
    Component* component = component_pair.second;
    if (component->Has(entity_id)) {
      component->Destroy(entity_id);
      ++destroyed_instances;
    }
  }
  return destroyed_instances;
}

int InstanceRegistry::DestroyInstanceFor(qbEntity entity,
                                          qbComponent component,
                                          GameState* state) {
  qbEntity entity_id = ENTITY_ID(entity);
  qbId table_id = ENTITY_TABLE_ID(entity);

  if (table_id) {
    EntityTable* table = table_registry_.Find(table_id);
    table->destroy_entity(entity);
  }

  Component* c = components_[component];
  if (c->Has(entity_id)) {
    SendInstanceDestroyNotification(entity, c, state);
    c->Destroy(entity_id);
    return 1;
  }
  return 0;
}

qbResult InstanceRegistry::SendInstanceCreateNotification(qbEntity entity, Component* component, GameState* state) const {
  return component_registry_.SendInstanceCreateNotification(entity, component, state);
}

qbResult InstanceRegistry::SendInstanceDestroyNotification(qbEntity entity, Component* component, GameState* state) const {
  return component_registry_.SendInstanceDestroyNotification(entity, component, state);
}

bool InstanceRegistry::InstanceHas(qbEntity entity, qbComponent component) {
  qbId table_id = ENTITY_TABLE_ID(entity);
  qbId entity_id = ENTITY_ID(entity);

  bool has_component = true;
  if (table_id) {
    EntityTable* table = table_registry_.Find(table_id);
    has_component = has_component || (table->has_entity(entity) && table->has_component(component));
  }
  has_component = has_component || (*this)[component].Has(entity_id);
  return has_component;
}

void* InstanceRegistry::InstanceData(qbEntity entity, qbComponent component) {
  qbId table_id = ENTITY_TABLE_ID(entity);
  qbId entity_id = ENTITY_ID(entity);

  bool has_component = true;
  if (table_id) {
    EntityTable* table = table_registry_.Find(table_id);
    if (table->has_component(component)) {
      return table->at(entity_id, component);
    }
  }
  return (*this)[component][entity];
}

size_t InstanceRegistry::InstanceCount(qbComponent component) {
  return (*this)[component].Size();
}

void InstanceRegistry::Lock(qbComponent component, bool is_mutable) {
  (*this)[component].Lock(is_mutable);
}

void InstanceRegistry::Unlock(qbComponent component, bool is_mutable) {
  (*this)[component].Unlock(is_mutable);
}