#include "system_impl.h"

SystemImpl::SystemImpl(const qbSystemAttr_& attr, qbSystem system) :
  system_(system), join_(attr.join), user_state_(attr.state),
  transform_(attr.transform),
  callback_(attr.callback) {
  for(auto* component : attr.sinks) {
    sinks_.push_back(component->impl);
    sink_interfaces_.push_back(component->impl->interface);
  }
  for(auto* component : attr.sources) {
    sources_.push_back(component->impl);
    source_interfaces_.push_back(component->impl->interface);
  }

  for(auto* source : sources_) {
    qbElement_ element;
    element.read_buffer = nullptr;
    element.user_buffer = nullptr;
    element.size = source->values.size;
    element.indexed_by = qbIndexedBy::QB_INDEXEDBY_KEY;

    elements_.push_back(element);
  }

  for (size_t i = 0; i < sources_.size(); ++i) {
    auto source = sources_[i];
    auto found = std::find(sinks_.begin(), sinks_.end(), source);
    if (found != sinks_.end()) {
      elements_[i].interface = (*found)->interface;
    }
    elements_data_.push_back(&elements_[i]);
  }
}

SystemImpl* SystemImpl::FromRaw(qbSystem system) {
  return (SystemImpl*)(((char*)system) + sizeof(qbSystem_));
}

void SystemImpl::Run(void* event) {
  size_t source_size = sources_.size();
  size_t sink_size = sinks_.size();
  qbFrame frame;
  frame.event = event;
  frame.state = system_->user_state;

  if (transform_) {
    if (source_size == 0 && sink_size == 0) {
      Run_0_To_0(&frame);
    } else if (source_size == 1 && sink_size == 0) {
      Run_1_To_0(&frame);
    } else if (source_size == 1 && sink_size > 0) {
      Run_1_To_N(&frame);
    } else if (source_size == 0 && sink_size > 0) {
      Run_0_To_N(&frame);
    } else if (source_size > 1) {
      Run_M_To_N(&frame);
    }
  }

  if (callback_) {
    callback_(sink_interfaces_.data(), &frame);
  }
}

void SystemImpl::CopyToElement(void* k, void* v, qbOffset offset,
                               qbElement element) {
  element->id = *(qbId*)k;
  element->read_buffer = v;
  element->offset = offset;
  element->indexed_by = qbIndexedBy::QB_INDEXEDBY_OFFSET;
}

void SystemImpl::CopyToElement(void* k, void* v, qbElement element) {
  element->id = *(qbId*)k;
  element->read_buffer = v;
  element->indexed_by = qbIndexedBy::QB_INDEXEDBY_KEY;
}

void SystemImpl::Run_0_To_0(qbFrame* f) {
  transform_(nullptr, nullptr, f);
}

void SystemImpl::Run_1_To_0(qbFrame* f) {
  qbCollection source = sources_[0];
  uint64_t count = source->count(&source->interface);
  uint8_t* keys = source->keys.data(&source->interface);
  uint8_t* values = source->values.data(&source->interface);

  for(uint64_t i = 0; i < count; ++i) {
    CopyToElement(
        (void*)(keys + source->keys.offset + i * source->keys.stride),
        (void*)(values + source->values.offset + i * source->values.stride),
        i, &elements_.front());
    transform_(elements_data_.data(), nullptr, f);
  }
}

void SystemImpl::Run_0_To_N(qbFrame* f) {
  transform_(nullptr, sink_interfaces_.data(), f);
}

void SystemImpl::Run_1_To_N(qbFrame* f) {
  qbCollection source = sources_[0];
  const uint64_t count = source->count(&source->interface);
  const uint8_t* keys = source->keys.data(&source->interface);
  const uint8_t* values = source->values.data(&source->interface);
  for(uint64_t i = 0; i < count; ++i) {
    CopyToElement(
        (void*)(keys + source->keys.offset + i * source->keys.stride),
        (void*)(values + source->values.offset + i * source->values.stride),
        i, &elements_.front());
    transform_(elements_data_.data(), sink_interfaces_.data(), f);
  }
}

void SystemImpl::Run_M_To_N(qbFrame* f) {
  qbCollection source = sources_[0];
  switch(join_) {
    case qbComponentJoin::QB_JOIN_INNER: {
      uint64_t min = 0xFFFFFFFFFFFFFFFF;
      for (size_t i = 0; i < sources_.size(); ++i) {
        qbCollection src = sources_[i];
        uint64_t maybe_min = src->count(&src->interface);
        if (maybe_min < min) {
          min = maybe_min;
          source = src;
        }
      }
    } break;
    case qbComponentJoin::QB_JOIN_LEFT:
      source = sources_[0];
    break;
    case qbComponentJoin::QB_JOIN_CROSS: {
      uint64_t* max_counts =
          (uint64_t*)alloca(sizeof(uint64_t) * sources_.size());
      uint64_t* indices =
          (uint64_t*)alloca(sizeof(uint64_t) * sources_.size());
      uint8_t** keys = (uint8_t**)alloca(sizeof(uint8_t*) * sources_.size());
      uint8_t** values = (uint8_t**)alloca(sizeof(uint8_t*) * sources_.size());

      memset(indices, 0, sizeof(uint64_t) * sources_.size());
      uint32_t indices_to_inc = 1;
      uint32_t max_index = 1 << sources_.size();
      bool all_empty = true;
      for (size_t i = 0; i < sources_.size(); ++i) {
        keys[i] = sources_[i]->keys.data(&sources_[i]->interface);
        values[i] = sources_[i]->values.data(&sources_[i]->interface);
        max_counts[i] = sources_[i]->count(&sources_[i]->interface);
        indices[i] = 0;
        all_empty &= sources_[i]->count(&sources_[i]->interface) == 0;
      }

      if (all_empty) {
        return;
      }

      while (indices_to_inc < max_index) {
        bool reached_max = false;
        size_t max_src = 0;
        for (size_t i = 0; i < sources_.size(); ++i) {
          reached_max |= indices[i] == max_counts[i];
          if (indices[i] == max_counts[i]) {
            max_src = i;
            continue;
          }

          qbCollection src = sources_[i];
          uint64_t index = indices[i];
          void* k =
              (void*)(keys[i] + src->keys.offset + index * src->keys.stride);
          void* v =
              (void*)(values[i] + src->values.offset + index * src->values.stride);
          CopyToElement(k, v, index, &elements_[i]);

          indices[i] += (1 << i) & indices_to_inc ? 1 : 0;
        }

        if (reached_max) {
          ++indices_to_inc;
          indices[max_src] = 0;
        }
        transform_(elements_data_.data(), sink_interfaces_.data(), f);
      }
    } return;
  }

  const uint64_t count = source->count(&source->interface);
  const uint8_t* keys = source->keys.data(&source->interface);
  switch(join_) {
    case qbComponentJoin::QB_JOIN_LEFT:
    case qbComponentJoin::QB_JOIN_INNER: {
      for(uint64_t i = 0; i < count; ++i) {
        void* k =
            (void*)(keys + source->keys.offset + i * source->keys.stride);

        bool should_continue = false;
        for (size_t j = 0; j < sources_.size(); ++j) {
          qbCollection c = sources_[j];
          void* v = c->interface.by_id(&c->interface, *(qbId*)k);
          if (!v) {
            should_continue = true;
            break;
          }
          CopyToElement(k, v, &elements_[j]);
        }
        if (should_continue) continue;

        transform_(elements_data_.data(), sink_interfaces_.data(), f);
      }
    } break;
    default:
      break;
  }
}

