#include "event_registry.h"

EventRegistry::EventRegistry(qbId program): program_(program), event_id_(0) { }

qbResult EventRegistry::CreateEvent(qbEvent* event, qbEventAttr attr) {
  std::lock_guard<decltype(state_mutex_)> lock(state_mutex_);
  qbId event_id = event_id_++;
  events_[event_id] = new Channel(attr->message_size);
  AllocEvent(event_id, event, events_[event_id]);
  return qbResult::QB_OK;
}

void EventRegistry::Subscribe(qbEvent event, qbSystem system) {
  std::lock_guard<decltype(state_mutex_)> lock(state_mutex_);
  FindEvent(event)->AddHandler(system);
}

void EventRegistry::Unsubscribe(qbEvent event, qbSystem system) {
  std::lock_guard<decltype(state_mutex_)> lock(state_mutex_);
  FindEvent(event)->RemoveHandler(system);
}

void EventRegistry::FlushAll() {
  for (auto event : events_) {
    event.second->Flush();
  }
}

void EventRegistry::Flush(qbEvent event) {
  Channel* queue = FindEvent(event);
  if (queue) {
    queue->Flush();
  }
}

void EventRegistry::AllocEvent(qbId id, qbEvent* event, Channel* channel) {
  *event = (qbEvent)calloc(1, sizeof(qbEvent_));
  *(qbId*)(&(*event)->id) = id;
  *(qbId*)(&(*event)->program) = program_;
  (*event)->channel = channel;
}

Channel* EventRegistry::FindEvent(qbEvent event) {
  auto it = events_.find(event->id);
  if (it != events_.end()) {
    return it->second;
  }
  return nullptr;
}
