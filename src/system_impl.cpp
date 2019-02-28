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
    instance.system = system_;
    if (std::find(attr.constants.begin(), attr.constants.end(), component) != attr.constants.end()) {
      *(bool*)&instance.is_mutable = false;
    } else {
      *(bool*)&instance.is_mutable = true;
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

void SystemImpl::Run(GameState* game_state, void* event) {
  size_t source_size = components_.size();
  qbFrame frame;
  frame.system = system_;
  frame.event = event;
  frame.state = system_->user_state;

  if (condition_ && !condition_(&frame)) {
    return;
  }

  if (transform_) {
    for (auto& t: tickets_) {
      t->lock();
    }
    if (source_size == 0) {
      Run_0(&frame);
    } else if (source_size == 1) {
      Component* c = game_state->ComponentGet(components_[0]);
      c->Lock(instances_[0].is_mutable);
      Run_1(c, &frame, game_state);
      c->Unlock(instances_[0].is_mutable);
    } else if (source_size > 1) {
      thread_local static std::vector<Component*> components;
      components.resize(0);
      size_t index = 0;
      for (auto component : components_) {
        Component* c = game_state->ComponentGet(component);
        c->Lock(instances_[index].is_mutable);
        components.push_back(c);
        c->Unlock(instances_[index].is_mutable);
        ++index;
      }
      Run_N(components, &frame, game_state);
    }
    for (auto& t : tickets_) {
      t->unlock();
    }
  }

  if (callback_) {
    callback_(&frame);
  }
}

qbInstance_ SystemImpl::FindInstance(qbEntity entity, Component* component, GameState* state) {
  qbInstance_ instance;
  instance.system = system_;
  CopyToInstance(component, entity, &instance, state);
  return instance;
}

void SystemImpl::CopyToInstance(Component* component, qbEntity entity, qbInstance instance, GameState* state) {
  CopyToInstance(component, entity, (*component)[entity], instance, state);
}

void SystemImpl::CopyToInstance(Component* component, qbEntity entity, void* instance_data, qbInstance instance, GameState* state) {
  instance->entity = entity;
  instance->data = instance_data;
  instance->component = component;
  instance->state = state;
}

void SystemImpl::RunTransform(qbInstance* instances, qbFrame* frame) {
  transform_(instances, frame);
}

void SystemImpl::Run_0(qbFrame* f) {
  RunTransform(nullptr, f);
}

void SystemImpl::Run_1(Component* component, qbFrame* f, GameState* state) {
  for (auto id_component : *component) {
    CopyToInstance(component, id_component.first, id_component.second, &instances_[0], state);
    RunTransform(instance_data_.data(), f);
  }
}

void SystemImpl::Run_N(const std::vector<Component*>& components, qbFrame* f, GameState* state) {
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
          CopyToInstance(src, (*it).first, &instances_[i], state);
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
          CopyToInstance(c, entity_id, &instances_[j], state);
        }
        if (should_continue) continue;

        RunTransform(instance_data_.data(), f);
      }
    } break;
    default:
      break;
  }
}

