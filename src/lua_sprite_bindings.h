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

#ifndef CUBEZ_LUA_SPRITE_BINDINGS__H
#define CUBEZ_LUA_SPRITE_BINDINGS__H

struct lua_State;

int sprite_load(lua_State* L);
int spritesheet_load(lua_State* L);
int sprite_fromsheet(lua_State* L);
int sprite_draw(lua_State* L);
int sprite_draw_ext(lua_State* L);
int sprite_drawpart(lua_State* L);
int sprite_drawpart_ext(lua_State* L);
int sprite_getoffset(lua_State* L);
int sprite_setoffset(lua_State* L);
int sprite_width(lua_State* L);
int sprite_height(lua_State* L);
int sprite_setdepth(lua_State* L);
int sprite_getdepth(lua_State* L);
int sprite_framecount(lua_State* L);
int sprite_getframe(lua_State* L);
int sprite_setframe(lua_State* L);

int animation_create(lua_State *L);
int animation_loaddir(lua_State* L);
int animation_fromsheet(lua_State* L);
int animation_getoffset(lua_State* L);
int animation_setoffset(lua_State* L);
int animation_play(lua_State* L);

#endif  // CUBEZ_LUA_SPRITE_BINDINGS__H