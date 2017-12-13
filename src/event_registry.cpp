#include "event_registry.h"

EventRegistry::EventRegistry(qbId program)
  : program_(program),
    event_id_(0),
    message_queue_(
      new std::queue<Channel::ChannelMessage>()) { }
EventRegistry::~EventRegistry() {
}

qbResult EventRegistry::CreateEvent(qbEvent* event, qbEventAttr attr) {
  std::lock_guard<decltype(state_mutex_)> lock(state_mutex_);
  qbId event_id = event_id_++;
  events_[event_id] = new Channel(event_id, message_queue_, attr->message_size);
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
  Channel::ChannelMessage msg;
  while (!message_queue_->empty()) {
    msg = message_queue_->front();
    events_[msg.handler]->Flush(msg.message);
    message_queue_->pop();
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
