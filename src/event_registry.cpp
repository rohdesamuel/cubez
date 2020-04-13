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

#include "event_registry.h"

EventRegistry::EventRegistry(qbId program)
  : program_(program),
    message_queue_(
      new ByteQueue(sizeof(Event::Message))) { }

EventRegistry::~EventRegistry() { }

qbResult EventRegistry::CreateEvent(qbEvent* event, qbEventAttr attr) {
  std::lock_guard<decltype(state_mutex_)> lock(state_mutex_);
  qbId event_id = events_.size();
  events_.push_back(new Event(program_, event_id, message_queue_,
                              attr->message_size));
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

void EventRegistry::FlushAll(GameState* state) {
  Event::Message msg;
  while (!message_queue_->empty()) {
    msg = *(Event::Message*)message_queue_->front();
    events_[msg.handler]->Flush(msg.index, state);
    message_queue_->pop();
  }
}

void EventRegistry::AllocEvent(qbId id, qbEvent* qb_event, Event* event) {
  *qb_event = (qbEvent)calloc(1, sizeof(qbEvent_));
  *(qbId*)(&(*qb_event)->id) = id;
  *(qbId*)(&(*qb_event)->program) = program_;
  (*qb_event)->event = event;
}

Event* EventRegistry::FindEvent(qbEvent event) {
  return events_[event->id];
}
