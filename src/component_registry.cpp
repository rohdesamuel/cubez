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

#include "component_registry.h"

#include "component.h"
#include "game_state.h"
ComponentRegistry::ComponentRegistry() : id_(0) {}

ComponentRegistry::~ComponentRegistry() {}

ComponentRegistry* ComponentRegistry::Clone() {
  ComponentRegistry* ret = new ComponentRegistry();
  long id = id_;
  ret->id_ = id;
  ret->components_defs_ = components_defs_;
  return ret;
}

qbResult ComponentRegistry::Create(qbComponent* component, qbComponentAttr attr) {
  qbId new_id = id_++;
  components_defs_[new_id] = *attr;
  components_[attr->name] = new_id;
  *component = new_id;  

  {
    instance_create_events_.resize(new_id + 1);
    qbEvent& on_create = instance_create_events_[new_id];

    qbEventAttr attr;
    qb_eventattr_create(&attr);
    qb_eventattr_setmessagetype(attr, qbInstanceOnCreateEvent_);
    qb_event_create(&on_create, attr);
    qb_eventattr_destroy(&attr);
  }
  {
    instance_destroy_events_.resize(new_id + 1);
    qbEvent& on_destroy = instance_destroy_events_[new_id];

    qbEventAttr attr;
    qb_eventattr_create(&attr);
    qb_eventattr_setmessagetype(attr, qbInstanceOnDestroyEvent_);
    qb_event_create(&on_destroy, attr);
    qb_eventattr_destroy(&attr);
  }

  return QB_OK;
}

Component* ComponentRegistry::Create(qbComponent component) const {
  const qbComponentAttr_& attr = components_defs_[component];
  return new Component(component, attr.data_size, attr.is_shared, attr.type);
}

void ComponentRegistry::RegisterSchema(qbComponent component, qbSchema schema) {
  auto& attr = components_defs_[component];
  attr.schema = schema;
  schemas_[attr.name] = schema;
}

qbComponent ComponentRegistry::Find(const std::string& name) const {
  auto it = components_.find(name);
  if (it == components_.end()) {
    return -1;
  }
  return it->second;
}

qbSchema ComponentRegistry::FindSchema(const std::string& name) const {
  auto it = schemas_.find(name);
  if (it == schemas_.end()) {
    return nullptr;
  }
  return it->second;
}

qbSchema ComponentRegistry::FindSchema(qbComponent component) const {
  return components_defs_[component].schema;
}

qbResult ComponentRegistry::SubcsribeToOnCreate(qbSystem system,
                                                qbComponent component) {
  Create(component);
  return qb_event_subscribe(
    instance_create_events_[component], system);
}

qbResult ComponentRegistry::SubcsribeToOnDestroy(qbSystem system,
                                                 qbComponent component) {
  Create(component);
  return qb_event_subscribe(
    instance_destroy_events_[component], system);
}

qbResult ComponentRegistry::SendInstanceCreateNotification(
  qbEntity entity, Component* component, GameState* state) const {
  qbInstanceOnCreateEvent_ event;
  event.entity = entity;
  event.component = component;
  event.state = state;

  return qb_event_sendsync(
    instance_create_events_[component->Id()], &event);
}

qbResult ComponentRegistry::SendInstanceDestroyNotification(
  qbEntity entity, Component* component, GameState* state) const {
  qbInstanceOnDestroyEvent_ event;
  event.entity = entity;
  event.component = component;
  event.state = state;

  return qb_event_sendsync(
    instance_destroy_events_[component->Id()], &event);
}