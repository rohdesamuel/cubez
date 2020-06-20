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

#include "program_impl.h"
#include "system_impl.h"

ProgramImpl::ProgramImpl(qbProgram* program)
    : program_(program),
      events_(program->id) {}

ProgramImpl* ProgramImpl::FromRaw(qbProgram* program) {
  return (ProgramImpl*)program->self;
}

qbId ProgramImpl::Id() const {
  return program_->id;
}

const char* ProgramImpl::Name() {
  return program_->name;
}

qbSystem ProgramImpl::CreateSystem(const qbSystemAttr_& attr) {
  for (qbComponent component : attr.mutables) {
    mutables_.insert(component);
  }

  qbSystem system = AllocSystem(systems_.size(), attr);
  systems_.push_back(system);
  EnableSystem(system);
  return system;
}

qbResult ProgramImpl::FreeSystem(qbSystem system) {
  return DisableSystem(system);
}

qbResult ProgramImpl::EnableSystem(qbSystem system) {
  DisableSystem(system);

  if (system->policy.trigger == qbTrigger::QB_TRIGGER_LOOP) {
    loop_systems_.push_back(system);
    std::sort(loop_systems_.begin(), loop_systems_.end());
  } else if (system->policy.trigger == qbTrigger::QB_TRIGGER_EVENT) {
    event_systems_.insert(system);
  } else {
    return QB_ERROR_UNKNOWN_TRIGGER_POLICY;
  }

  return QB_OK;
}

qbResult ProgramImpl::DisableSystem(qbSystem system) {
  auto found = std::find(loop_systems_.begin(), loop_systems_.end(), system);
  if (found != loop_systems_.end()) {
    loop_systems_.erase(found);
  } else {
    event_systems_.erase(system);
  }
  return QB_OK;
}

bool ProgramImpl::HasSystem(qbSystem system) {
  return std::find(systems_.begin(), systems_.end(), system) != systems_.end();
}

qbResult ProgramImpl::CreateEvent(qbEvent* event, qbEventAttr attr) {
  return events_.CreateEvent(event, attr);
}

void ProgramImpl::FlushAllEvents(GameState* state) {
  events_.FlushAll(state);
}

void ProgramImpl::SubscribeTo(qbEvent event, qbSystem system) {
  events_.Subscribe(event, system);
}

void ProgramImpl::UnsubscribeFrom(qbEvent event, qbSystem system) {
  events_.Unsubscribe(event, system);
}

void ProgramImpl::Ready() {
  // Give copy of components.
}

void ProgramImpl::Run(GameState* state, lua_State* lua_state) {
  events_.FlushAll(state);
  for(qbSystem p : loop_systems_) {
    SystemImpl::FromRaw(p)->Run(state);
  }
  lua_update(lua_state);
}

void ProgramImpl::Done() {
  // Merge changes back into the base components.
}

qbSystem ProgramImpl::AllocSystem(qbId id, const qbSystemAttr_& attr) {
  qbSystem p = (qbSystem)calloc(1, sizeof(qbSystem_) + sizeof(SystemImpl));
  *(qbId*)(&p->id) = id;
  *(qbId*)(&p->program) = program_->id;
  p->policy.trigger = attr.trigger;
  p->policy.priority = attr.priority;
  p->user_state = attr.state;

  SystemImpl* impl = SystemImpl::FromRaw(p);

  new (impl) SystemImpl(attr, p, attr.components);

  return p;
}

