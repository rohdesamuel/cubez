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

#ifndef CUBEZ_GUI_BINDINGS__H
#define CUBEZ_GUI_BINDINGS__H

struct lua_State;

/*
*  Returns:
*    table[guielement: qb.GuiElement*]
*/
int guielement_getfocus(lua_State* L);

/*
*  Args:
*    array[integer]: x, y position
*  Returns:
*    table[guielement: qb.GuiElement*]
*/
int guielement_getfocusat(lua_State* L);

/*
*  Args:
*    string: id of gui guielement
*  Returns:
*    table[guielement: qb.GuiElement*]
*/
int guielement_find(lua_State* L);
int guielement_create(lua_State* L);
int guielement_destroy(lua_State* L);
int guielement_open(lua_State* L);
int guielement_close(lua_State* L);
int guielement_closechildren(lua_State* L);
int guielement_setconstraint(lua_State* L);
int guielement_getconstraint(lua_State* L);
int guielement_clearconstraint(lua_State* L);
int guielement_link(lua_State* L);
int guielement_unlink(lua_State* L);
int guielement_getparent(lua_State* L);
int guielement_movetofront(lua_State* L);
int guielement_movetoback(lua_State* L);
int guielement_moveforward(lua_State* L);
int guielement_movebackward(lua_State* L);
int guielement_moveto(lua_State* L);
int guielement_moveby(lua_State* L);
int guielement_resizeto(lua_State* L);
int guielement_resizeby(lua_State* L);
int guielement_getsize(lua_State* L);
int guielement_getposition(lua_State* L);
int guielement_setvalue(lua_State* L);
int guielement_getvalue(lua_State* L);
int guielement_gettext(lua_State* L);
int guielement_settext(lua_State* L);
int guielement_settextcolor(lua_State* L);
int guielement_settextscale(lua_State* L);
int guielement_settextsize(lua_State* L);
int guielement_settextalign(lua_State* L);

#endif  // CUBEZ_GUI_BINDINGS__H