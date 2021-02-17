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

#ifndef CUBEZ_LUA_AUDIO_BINDINGS__H
#define CUBEZ_LUA_AUDIO_BINDINGS__H

struct lua_State;

int audio_loadwav(lua_State* L);
int audio_free(lua_State* L);
int audio_isplaying(lua_State* L);
int audio_play(lua_State* L);
int audio_stop(lua_State* L);
int audio_pause(lua_State* L);
int audio_setloop(lua_State* L);
int audio_setpan(lua_State* L);
int audio_setvolume(lua_State* L);
int audio_stopall(lua_State* L);

#endif  // CUBEZ_LUA_AUDIO_BINDINGS__H