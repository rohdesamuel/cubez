#include "system_impl.h"

SystemImpl::SystemImpl(ComponentRegistry* component_registry,
                       const qbSystemAttr_& attr, qbSystem system) :
  component_registry_(component_registry), system_(system), join_(attr.join),
  user_state_(attr.state), transform_(attr.transform),
  callback_(attr.callback) {

  for (auto* component : attr.sources) {
    sources_.push_back(component->id);
  }
  for(auto* component : attr.sinks) {
    sinks_.push_back(component->id);
  }

  for(qbId source : sources_) {
    qbElement_ element;
    element.read_buffer = nullptr;
    element.user_buffer = nullptr;
    element.size = (*component_registry_)[source].instances.element_size();

    elements_.push_back(element);
  }

  for (size_t i = 0; i < sources_.size(); ++i) {
    auto source = sources_[i];
    auto found = std::find(sinks_.begin(), sinks_.end(), source);
    if (found != sinks_.end()) {
      elements_[i].component = nullptr;
    }
    elements_data_.push_back(&elements_[i]);
  }
}

SystemImpl* SystemImpl::FromRaw(qbSystem system) {
  return (SystemImpl*)(((char*)system) + sizeof(qbSystem_));
}

void SystemImpl::Run(void* event) {
  size_t source_size = sources_.size();
  qbFrame frame;
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

void SystemImpl::CopyToElement(qbComponent component, qbId instance_id,
                               void* instance_data, qbElement element) {
  element->id = instance_id;
  element->read_buffer = instance_data;
  element->component = component;
}

void SystemImpl::Run_0(qbFrame* f) {
  transform_(nullptr, f);
}

void SystemImpl::Run_1(qbFrame* f) {
  qbComponent component = &(*component_registry_)[sources_[0]];
  auto& instances = component->instances;

  for (auto id_component : instances) {
    CopyToElement(component, id_component.first, id_component.second, &elements_[0]);
    transform_(elements_data_.data(), f);
  }
}

void SystemImpl::Run_N(qbFrame* f) {
  qbComponent source = &(*component_registry_)[sources_[0]];
  switch(join_) {
    case qbComponentJoin::QB_JOIN_INNER: {
      uint64_t min = 0xFFFFFFFFFFFFFFFF;
      for (size_t i = 0; i < sources_.size(); ++i) {
        qbComponent src = &(*component_registry_)[sources_[i]];
        uint64_t maybe_min = src->instances.size();
        if (maybe_min < min) {
          min = maybe_min;
          source = src;
        }
      }
    } break;
    case qbComponentJoin::QB_JOIN_LEFT:
      source = &(*component_registry_)[sources_[0]];
    break;
    case qbComponentJoin::QB_JOIN_CROSS: {
#if 1
    static std::vector<size_t> indices(sources_.size(), 0);

    while (1) {
      for (size_t i = 0; i < indices.size(); ++i) {
        qbId source_id = sources_[i];
        qbComponent src = &(*component_registry_)[source_id];
        auto it = src->instances.begin() + indices[i];
        CopyToElement(src, (*it).first, (*it).second, &elements_[i]);
      }
      transform_(elements_data_.data(), f);

      bool all_zero = true;
      ++indices[0];
      for (size_t i = 0; i < indices.size(); ++i) {
        if (indices[i] >= (*component_registry_)[sources_[i]].instances.size()) {
          indices[i] = 0;
          if (i + 1 < indices.size()) {
            ++indices[i + 1];
          }
        }
        all_zero &= indices[i] == 0;
      }

      if (all_zero) break;
    }
#else

      int i = 0;
      while(1) {
        int j = i;
        for (size_t index = 0; index < sources_.size(); ++index) {
          qbId source_id = sources_[index];
          qbComponent src = &(*component_registry_)[source_id];

          auto it = src->instances.begin();
          it = it + (j % std::max((uint64_t)1, src->instances.size()));

          std::cout << j << "\n";
          j /= std::max((uint64_t)1, src->instances.size());
          std::cout << j << "\n";

          CopyToElement(src, (*it).first, (*it).second, &elements_[index]);
        }
        if (j > 0) return;
        transform_(elements_data_.data(), f);
        ++i;
      }
#endif
    } return;
  }

  switch(join_) {
    case qbComponentJoin::QB_JOIN_LEFT:
    case qbComponentJoin::QB_JOIN_INNER: {
      for (auto id_component : source->instances) {
        qbId entity_id = id_component.first;
        bool should_continue = false;
        for (size_t j = 0; j < sources_.size(); ++j) {
          qbComponent c = &(*component_registry_)[sources_[j]];
          if (!c->instances.has(entity_id)) {
            should_continue = true;
            break;
          }
          CopyToElement(c, entity_id, c->instances[entity_id], &elements_[j]);
        }
        if (should_continue) continue;

        transform_(elements_data_.data(), f);
      }
    } break;
    default:
      break;
  }
}

