#include "event_registry.h"

EventRegistry::EventRegistry(qbId program)
  : program_(program),
    message_queue_(
      new ByteQueue(sizeof(Channel::ChannelMessage))) { }

EventRegistry::~EventRegistry() { }

qbResult EventRegistry::CreateEvent(qbEvent* event, qbEventAttr attr) {
  std::lock_guard<decltype(state_mutex_)> lock(state_mutex_);
  qbId event_id = events_.size();
  events_.push_back(new Channel(event_id, message_queue_, attr->message_size));
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
    msg = *(Channel::ChannelMessage*)message_queue_->front();
    events_[msg.handler]->Flush(msg.index);
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
  return events_[event->id];
}
