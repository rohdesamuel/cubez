/**
* Author: Samuel Rohde (rohde.samuel@cubez.io)
*
* Copyright {2020} {Samuel Rohde}
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

#ifndef SYSTEM_IMPL__H
#define SYSTEM_IMPL__H

#include "defs.h"
#include "game_state.h"
#include "barrier.h"

#include <algorithm>
#include <cstring>

class SystemImpl {
 public:
  SystemImpl(const qbSystemAttr_& attr, qbSystem system, std::vector<qbComponent> components);

  static SystemImpl* FromRaw(qbSystem system);

  void Run(GameState* game_state, void* event = nullptr);

  qbInstance_ FindInstance(qbEntity entity, Component* component, GameState* state);

 private:
  void CopyToInstance(Component* component, qbEntity entity, qbInstance instance, GameState* state);
  void CopyToInstance(Component* component, qbEntity entity, void* instance_data, qbInstance instance, GameState* state);

  void Run_0(qbFrame* f);
  void Run_1(Component* component, qbFrame* f, GameState* state);
  void Run_N(const std::vector<Component*>& components, qbFrame* f, GameState* state);

  void RunTransform(qbInstance* instances, qbFrame* frame);

  qbSystem system_;
  std::vector<qbComponent> components_;

  qbComponentJoin join_;
  void* user_state_;

  std::vector<qbInstance> instance_data_;
  std::vector<qbInstance_> instances_;  
  std::vector<qbTicket_*> tickets_;

  qbTransformFn transform_;
  qbCallbackFn callback_;
  qbConditionFn condition_;
};


#endif  // SYSTEM_IMPL__H

