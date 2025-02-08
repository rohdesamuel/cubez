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

#ifndef SYSTEM_IMPL__H
#define SYSTEM_IMPL__H

#include "defs.h"
#include "game_state.h"
#include "barrier.h"

#include <algorithm>
#include <cstring>
#include <stdarg.h>

class SystemImpl {
 public:
  SystemImpl(const qbSystemAttr_& attr, qbSystem system, std::vector<qbComponent> components);

  static SystemImpl* FromRaw(qbSystem system);
  static qbSystem ToRaw(SystemImpl* system);

  qbVar Run(GameState* game_state, void* event=nullptr, qbVar var=qbNil);

  qbInstance_ FindInstance(qbEntity entity, Component* component);

  void InstanceGet(GameState* game_state, qbInstance instance, va_list args);
  void InstanceGeti(GameState* game_state, qbInstance instance, size_t index, void* pbuf);
  void InstanceGetn(GameState* game_state, qbInstance instance, size_t count, void* pbufs[]);

private:
  void CopyToInstance(qbEntity entity, void* instance_data);
  void CopyToInstance(Component* component, qbEntity entity);
  void CopyToInstance(Component* component, qbEntity entity, size_t index);
  void CopyToInstance(void* pbuf, size_t index);

  void Run_0(qbFrame* f);
  void Run_1(Component* component, qbFrame* f);
  void Run_N(const std::vector<Component*>& components, qbFrame* f);

  void RunTransform(qbInstance instance, qbFrame* frame);

  qbSystem system_;
  std::vector<qbComponent> components_;

  qbComponentJoin join_;
  void* user_state_;

  qbInstance_ instance_;
  std::vector<void*> component_data_;
  std::vector<bool> component_ismutable_;
  std::vector<qbTicket_*> tickets_;

  qbTransformFn transform_;
  qbCallbackFn callback_;
  qbConditionFn condition_;
};


#endif  // SYSTEM_IMPL__H

