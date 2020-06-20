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

#ifndef PROGRAM_IMPL__H
#define PROGRAM_IMPL__H

#include "defs.h"
#include "component_registry.h"
#include "event_registry.h"
#include "game_state.h"
#include "lua_bindings.h"

class ProgramImpl {
 public:
  ProgramImpl(qbProgram* program);
  static ProgramImpl* FromRaw(qbProgram* program);

  qbSystem CreateSystem(const qbSystemAttr_& attr);

  qbResult FreeSystem(qbSystem system);

  qbResult EnableSystem(qbSystem system);

  qbResult DisableSystem(qbSystem system);

  bool HasSystem(qbSystem system);

  qbResult CreateEvent(qbEvent* event, qbEventAttr attr);

  void FlushAllEvents(GameState* state);

  void SubscribeTo(qbEvent event, qbSystem system);

  void UnsubscribeFrom(qbEvent event, qbSystem system);

  void Ready();
  void Run(GameState* state, lua_State* lua_state);
  void Done();

  qbId Id() const;
  const char* Name();

 private:
  qbSystem AllocSystem(qbId id, const qbSystemAttr_& attr);

  qbProgram* program_;
  EventRegistry events_;

  std::set<qbComponent> mutables_;
  std::vector<qbSystem> systems_;
  std::vector<qbSystem> loop_systems_;
  std::set<qbSystem> event_systems_;
};

#endif  // PROGRAM_IMPL__H

