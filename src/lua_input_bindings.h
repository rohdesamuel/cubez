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

#ifndef CUBEZ_LUA_INPUT_BINDINGS__H
#define CUBEZ_LUA_INPUT_BINDINGS__H

struct lua_State;

void lua_input_initialize();

/*
*  Args:
*    string: key
*  Returns:
*    boolean: if key is pressed
*/
int key_pressed(lua_State* L);

/*
*  Args:
*    string: scan code
*  Returns:
*    boolean: if key is pressed
*/
int scan_pressed(lua_State* L);

/*
*  Args:
*    string: mouse button 
*  Returns:
*    boolean: if mouse button is pressed
*/
int mouse_pressed(lua_State* L);

/*
*  Returns:
*    tuple[int: x, int: y]: the mouse position
*/
int mouse_position(lua_State* L);

/*
*  Returns:
*    tuple[int: x, int: y]: the mouse wheel state
*/
int mouse_wheel(lua_State* L);

/*
*  Args:
*    boolean: if true sets mouse to be in relative mode.
*/
int mouse_setrelative(lua_State* L);

/*
*  Returns:
*    boolean: if mouse is in relative mode
*/
int mouse_relative(lua_State* L);

/*
*  Returns:
*    Enum[app, gui]: where the engine is focused
*/
int userfocus(lua_State* L);

#endif  // CUBEZ_LUA_INPUT_BINDINGS__H