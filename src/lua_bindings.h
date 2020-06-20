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

#ifndef CUBEZ_LUA_BINDINGS__H
#define CUBEZ_LUA_BINDINGS__H

struct lua_State;

void lua_bindings_initialize(struct qbScriptAttr_* attr);

lua_State* lua_thread_initialize();

void lua_start(lua_State* L);
void lua_update(lua_State* L);

#endif  // CUBEZ_LUA_BINDINGS__H