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
InstanceRegistry::InstanceRegistry(const ComponentRegistry& component_registry) :
  component_registry_(component_registry) {}

InstanceRegistry::~InstanceRegistry() {
  for (auto c_pair : components_) {
    delete c_pair.second;
  }
}

InstanceRegistry* InstanceRegistry::Clone() {
  InstanceRegistry* ret = new InstanceRegistry(component_registry_);
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
  qbEntity entity, const std::vector<qbComponentInstance_>& instances,
  GameState* state) {
  for (auto& instance : instances) {
    Create(instance.component);
    Component* component = components_[instance.component];
    component->Create(entity, instance.data);
  }

  for (auto& instance : instances) {
    Component* component = components_[instance.component];
    SendInstanceCreateNotification(entity, component, state);
  }

  return QB_OK;
}

qbResult InstanceRegistry::CreateInstanceFor(qbEntity entity,
                                              qbComponent component,
                                              void* instance_data,
                                              GameState* state) {
  Create(component);
  Component* c = components_[component];
  c->Create(entity, instance_data);
  SendInstanceCreateNotification(entity, c, state);
  return QB_OK;
}

int InstanceRegistry::DestroyInstancesFor(qbEntity entity, GameState* state) {
  int destroyed_instances = 0;
  for (auto component_pair : components_) {
    Component* component = component_pair.second;
    if (component->Has(entity)) {
      SendInstanceDestroyNotification(entity, component, state);
    }
  }

  for (auto component_pair : components_) {
    Component* component = component_pair.second;
    if (component->Has(entity)) {
      component->Destroy(entity);
      ++destroyed_instances;
    }
  }
  return destroyed_instances;
}

int InstanceRegistry::DestroyInstanceFor(qbEntity entity,
                                          qbComponent component,
                                          GameState* state) {
  Component* c = components_[component];
  if (c->Has(entity)) {
    SendInstanceDestroyNotification(entity, c, state);
    c->Destroy(entity);
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