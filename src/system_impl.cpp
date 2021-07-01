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

#include "system_impl.h"
#include <omp.h>

SystemImpl::SystemImpl(const qbSystemAttr_& attr, qbSystem system, std::vector<qbComponent> components) :
  system_(system), 
  components_(components), join_(attr.join),
  user_state_(attr.state),
  tickets_(attr.tickets),
  transform_(attr.transform),
  callback_(attr.callback),
  condition_(attr.condition) {

  for(auto component : components_) {
    qbInstance_ instance;
    instance.data = nullptr;
    if (std::find(attr.constants.begin(), attr.constants.end(), component) != attr.constants.end()) {
      *(bool*)&instance.is_mutable = false;
    } else {
      *(bool*)&instance.is_mutable = true;
    }

    if (qb_component_schema(component)) {
      *(bool*)&instance.has_schema = true;
    } else {
      *(bool*)&instance.has_schema = false;
    }

    instances_.push_back(instance);
  }

  for (auto& element : instances_) {
    instance_data_.push_back(&element);
  }
}

SystemImpl* SystemImpl::FromRaw(qbSystem system) {
  return (SystemImpl*)(((char*)system) + sizeof(qbSystem_));
}

qbVar SystemImpl::Run(GameState* game_state, void* event, qbVar var) {
  qbVar ret = qbNil;
  size_t source_size = components_.size();
  qbFrame frame;
  frame.system = system_;
  frame.event = event;
  frame.state = system_ ? system_->user_state : user_state_;

  if (condition_ && !condition_(&frame)) {
    return ret;
  }

  if (transform_) {
    for (auto& t: tickets_) {
      t->lock();
    }
    if (source_size == 0) {
      Run_0(&frame);
    } else if (source_size == 1) {
      game_state->ComponentLock(components_[0], instances_[0].is_mutable);
      Component* c = game_state->ComponentGet(components_[0]);
      c->Lock(instances_[0].is_mutable);
      Run_1(c, &frame);
      c->Unlock(instances_[0].is_mutable);
    } else if (source_size > 1) {
      thread_local static std::vector<Component*> components;
      components.resize(0);
      size_t index = 0;
      for (auto component : components_) {
        Component* c = game_state->ComponentGet(component);
        c->Lock(instances_[index].is_mutable);
        components.push_back(c);        
        ++index;
      }

      Run_N(components, &frame);

      index = 0;
      for (auto component : components) {
        component->Unlock(instances_[index].is_mutable);
        ++index;
      }
    }
    for (auto& t : tickets_) {
      t->unlock();
    }
  }
  
  if (callback_) {
    ret = callback_(&frame, var);
  }
  return ret;
}

qbInstance_ SystemImpl::FindInstance(qbEntity entity, Component* component) {
  qbInstance_ instance;
  CopyToInstance(component, entity, &instance);
  return instance;
}

void SystemImpl::CopyToInstance(Component* component, qbEntity entity,
                                qbInstance instance) {
  CopyToInstance(component, entity, (*component)[entity], instance);
}

void SystemImpl::CopyToInstance(Component* component, qbEntity entity,
                                void* instance_data, qbInstance instance) {
  instance->entity = entity;
  instance->data = instance_data;
  instance->component = component;
}

void SystemImpl::RunTransform(qbInstance* instances, qbFrame* frame) {
  transform_(instances, frame);
}

void SystemImpl::Run_0(qbFrame* f) {
  RunTransform(nullptr, f);
}

void SystemImpl::Run_1(Component* component, qbFrame* f) {
  for (auto id_component : *component) {
    CopyToInstance(component, id_component.first, id_component.second, &instances_[0]);
    RunTransform(instance_data_.data(), f);
  }
}

void SystemImpl::Run_N(const std::vector<Component*>& components, qbFrame* f) {
  Component* source = components[0];
  switch(join_) {
    case qbComponentJoin::QB_JOIN_INNER: {
      uint64_t min = 0xFFFFFFFFFFFFFFFF;
      for (size_t i = 0; i < components_.size(); ++i) {
        Component* src = components[i];
        uint64_t maybe_min = src->Size();
        if (maybe_min < min) {
          min = maybe_min;
          source = src;
        }
      }
    } break;
    case qbComponentJoin::QB_JOIN_LEFT:
      source = components[0];
    break;
    case qbComponentJoin::QB_JOIN_CROSS: {
      static std::vector<size_t> indices(components_.size(), 0);

      for (Component* component : components) {
        if (component->Size() == 0) {
          return;
        }
      }

      while (1) {
        for (size_t i = 0; i < indices.size(); ++i) {
          Component* src = components[i];
          auto it = src->begin() + indices[i];
          CopyToInstance(src, (*it).first, &instances_[i]);
        }
        RunTransform(instance_data_.data(), f);

        bool all_zero = true;
        ++indices[0];
        for (size_t i = 0; i < indices.size(); ++i) {
          if (indices[i] >= components[i]->Size()) {
            indices[i] = 0;
            if (i + 1 < indices.size()) {
              ++indices[i + 1];
            }
          }
          all_zero &= indices[i] == 0;
        }

        if (all_zero) break;
      }
    }
    case qbComponentJoin::QB_JOIN_UNION:
    {
      std::unordered_set<qbId> seen;

      for (auto c_outer : components) {
        for (auto id_component : *c_outer) {
          const qbId entity_id = id_component.first;
          if (seen.find(entity_id) != seen.end()) {
            continue;
          }
          seen.insert(entity_id);

          for (size_t i = 0; i < components_.size(); ++i) {
            Component* c = components[i];
            if (c->Has(entity_id)) {
              CopyToInstance(c, entity_id, &instances_[i]);
            } else {
              CopyToInstance(c, entity_id, nullptr, &instances_[i]);
            }
          }

          RunTransform(instance_data_.data(), f);
        }
      }
    }
  }

  switch(join_) {
    case qbComponentJoin::QB_JOIN_LEFT:
    case qbComponentJoin::QB_JOIN_INNER: {
      for (auto id_component : *source) {
        qbId entity_id = id_component.first;
        bool should_continue = false;
        for (size_t j = 0; j < components_.size(); ++j) {
          Component* c = components[j];
          if (!c->Has(entity_id)) {
            should_continue = true;
            break;
          }
          CopyToInstance(c, entity_id, &instances_[j]);
        }
        if (should_continue) continue;

        RunTransform(instance_data_.data(), f);
      }
    } break;
    default:
      break;
  }
}

