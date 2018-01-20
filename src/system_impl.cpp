#include "system_impl.h"

SystemImpl::SystemImpl(const qbSystemAttr_& attr, qbSystem system) :
  system_(system), join_(attr.join),
  user_state_(attr.state), transform_(attr.transform),
  callback_(attr.callback) {

  components_ = std::move(attr.components);

  size_t i = 0;
  for(auto component : components_) {
    qbInstance_ instance;
    instance.data = nullptr;
    instance.system = system_;
    if (std::find(attr.constants.begin(), attr.constants.end(), component) != attr.constants.end()) {
      instance.is_mutable = false;
    } else {
      instance.is_mutable = true;
    }

    instances_.push_back(instance);
    ++i;
  }

  for (auto& element : instances_) {
    instance_data_.push_back(&element);
  }
}

SystemImpl* SystemImpl::FromRaw(qbSystem system) {
  return (SystemImpl*)(((char*)system) + sizeof(qbSystem_));
}

void SystemImpl::Run(void* event) {
  size_t source_size = components_.size();
  qbFrame frame;
  frame.system = system_;
  frame.event = event;
  frame.state = system_->user_state;

  if (transform_) {
    if (source_size == 0) {
      Run_0(&frame);
    } else if (source_size == 1) {
      Run_1(&frame);
    } else if (source_size > 1) {
      Run_N(&frame);
    }
  }

  if (callback_) {
    callback_(&frame);
  }
}

void* SystemImpl::FindInstanceData(qbInstance instance, qbComponent component) {
  return component->instances[instance->entity];
}

qbInstance_ SystemImpl::FindInstance(qbEntity entity, qbComponent component) {
  qbInstance_ instance;
  instance.is_mutable = false;
  instance.system = system_;
  CopyToInstance(component, entity, &instance);
  return instance;
}

void SystemImpl::CopyToInstance(qbComponent component, qbEntity entity, qbInstance instance) {
  instance->entity = entity;
  instance->data = instance->is_mutable
      ? component->instances[entity]
      : (void*)component->instances.at(entity);
  instance->component = component;
}

void SystemImpl::Run_0(qbFrame* f) {
  transform_(nullptr, f);
}

void SystemImpl::Run_1(qbFrame* f) {
  qbComponent component = components_[0];
  auto& instances = component->instances;

  for (auto id_component : instances) {
    CopyToInstance(component, id_component.first, &instances_[0]);
    transform_(instance_data_.data(), f);
  }
}

void SystemImpl::Run_N(qbFrame* f) {
  qbComponent source = components_[0];
  switch(join_) {
    case qbComponentJoin::QB_JOIN_INNER: {
      uint64_t min = 0xFFFFFFFFFFFFFFFF;
      for (size_t i = 0; i < components_.size(); ++i) {
        qbComponent src = components_[i];
        uint64_t maybe_min = src->instances.Size();
        if (maybe_min < min) {
          min = maybe_min;
          source = src;
        }
      }
    } break;
    case qbComponentJoin::QB_JOIN_LEFT:
      source = components_[0];
    break;
    case qbComponentJoin::QB_JOIN_CROSS: {
      static std::vector<size_t> indices(components_.size(), 0);

      for (qbComponent component : components_) {
        if (component->instances.Size() == 0) {
          return;
        }
      }

      while (1) {
        for (size_t i = 0; i < indices.size(); ++i) {
          qbComponent src = components_[i];
          auto it = src->instances.begin() + indices[i];
          CopyToInstance(src, (*it).first, &instances_[i]);
        }
        transform_(instance_data_.data(), f);

        bool all_zero = true;
        ++indices[0];
        for (size_t i = 0; i < indices.size(); ++i) {
          if (indices[i] >= components_[i]->instances.Size()) {
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
      for (auto id_component : source->instances) {
        qbId entity_id = id_component.first;
        bool should_continue = false;
        for (size_t j = 0; j < components_.size(); ++j) {
          qbComponent c = components_[j];
          if (!c->instances.Has(entity_id)) {
            should_continue = true;
            break;
          }
          CopyToInstance(c, entity_id, &instances_[j]);
        }
        if (should_continue) continue;

        transform_(instance_data_.data(), f);
      }
    } break;
    default:
      break;
  }
}

