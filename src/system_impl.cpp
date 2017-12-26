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

void SystemImpl::CopyToElement(qbComponent component, qbId instance_id, void* instance_data, qbElement element) {
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
    } return;
  }

  switch(join_) {
    case qbComponentJoin::QB_JOIN_LEFT:
    case qbComponentJoin::QB_JOIN_INNER: {
      for (auto id_component : source->instances) {
        bool should_continue = false;
        for (size_t j = 0; j < sources_.size(); ++j) {
          qbComponent c = &(*component_registry_)[j];
          if (!c->instances.has(id_component.first)) {
            should_continue = true;
            break;
          }
          CopyToElement(c, id_component.first, id_component.second, &elements_[j]);
        }
        if (should_continue) continue;

        transform_(elements_data_.data(), f);
      }
    } break;
    default:
      break;
  }
}

