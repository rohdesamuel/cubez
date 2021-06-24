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

#include "lua_gui_bindings.h"
#include "lua_common.h"

#include <cubez/gui.h>
#include <iostream>
#include <unordered_map>
#include <mutex>

extern "C" {
#include <lua/lua.h>
#include <lua/lualib.h>
#include <lua/lauxlib.h>
}

namespace
{

qbTextAlign align_string_to_enum(const std::string& align) {
  if (align == "left") {
    return qbTextAlign::QB_TEXT_ALIGN_LEFT;
  } else if (align == "center") {
    return qbTextAlign::QB_TEXT_ALIGN_CENTER;
  } else if (align == "right") {
    return qbTextAlign::QB_TEXT_ALIGN_RIGHT;
  }
  return qbTextAlign::QB_TEXT_ALIGN_LEFT;
}

qbGuiProperty property_string_to_enum(const std::string& property) {
  if (property == "x") {
    return QB_GUI_X;
  } else if (property == "y") {
    return QB_GUI_Y;
  } else if (property == "width") {
    return QB_GUI_WIDTH;
  } else if (property == "height") {
    return QB_GUI_HEIGHT;
  }
  assert(false && "Unknown property");
}

qbConstraint constraint_string_to_enum(const std::string& constraint) {
  if (constraint == "pixel") {
    return QB_CONSTRAINT_PIXEL;
  } else if (constraint == "percent") {
    return QB_CONSTRAINT_PERCENT;
  } else if (constraint == "relative") {
    return QB_CONSTRAINT_RELATIVE;
  } else if (constraint == "min") {
    return QB_CONSTRAINT_MIN;
  } else if (constraint == "max") {
    return QB_CONSTRAINT_MAX;
  } else if (constraint == "aspectratio") {
    return QB_CONSTRAINT_ASPECT_RATIO;
  }
  assert(false && "Unknown constraint");
}

}

struct GuiLuaState {
  lua_State* L;
};

std::mutex gui_elements_mu;
std::unordered_map<qbGuiElement, qbLuaReference> gui_elements;

int guielement_getfocus(lua_State* L) {
  qbGuiElement el = qb_guielement_getfocus();
  {
    std::lock_guard<decltype(gui_elements_mu)> l(gui_elements_mu);
    lua_rawgeti(L, LUA_REGISTRYINDEX, gui_elements[el].ref);
  }
  return 1;
}

int guielement_getfocusat(lua_State* L) {
  float x = (float)lua_tonumber(L, 1);
  float y = (float)lua_tonumber(L, 2);

  qbGuiElement el = qb_guielement_getfocusat(x, y);
  {
    std::lock_guard<decltype(gui_elements_mu)> l(gui_elements_mu);
    lua_rawgeti(L, LUA_REGISTRYINDEX, gui_elements[el].ref);
  }
  return 1;
}

int guielement_find(lua_State* L) {
  std::string id = lua_tostring(L, 1);
  qbGuiElement el = qb_guielement_find(id.data());

  {
    std::lock_guard<decltype(gui_elements_mu)> l(gui_elements_mu);
    lua_rawgeti(L, LUA_REGISTRYINDEX, gui_elements[el].ref);
  }

  return 1;
}

int guielement_create(lua_State* L) {
  const int ID_ARG = 1;
  const int OPT_ARG = 2;

  const char* id = lua_tostring(L, ID_ARG);
  qbGuiElementAttr_ attr = {};

  if (lua_getfield(L, OPT_ARG, "backgroundcolor") != LUA_TNIL) {
    int background_color = lua_gettop(L);
    if (lua_rawlen(L, background_color) != 4) {
      std::cout << "Incorrect number of arguments for background color" << std::endl;
    }

    lua_geti(L, background_color, 1);
    lua_geti(L, background_color, 2);
    lua_geti(L, background_color, 3);
    lua_geti(L, background_color, 4);

    attr.background_color.x = (float)lua_tonumber(L, -4);
    attr.background_color.y = (float)lua_tonumber(L, -3);
    attr.background_color.z = (float)lua_tonumber(L, -2);
    attr.background_color.w = (float)lua_tonumber(L, -1);

    // 4 for background color fields, 1 for getfield.
    lua_pop(L, 5);
  }

  if (lua_getfield(L, OPT_ARG, "background") != LUA_TNIL) {
    attr.background = (qbImage)lua_touserdata(L, -1);
    lua_pop(L, 1);
  }

  if (lua_getfield(L, OPT_ARG, "radius") != LUA_TNIL) {
    attr.radius = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);
  }

  if (lua_getfield(L, OPT_ARG, "offset") != LUA_TNIL) {
    int offset = lua_gettop(L);
    if (lua_rawlen(L, offset) != 2) {
      std::cout << "Incorrect number of arguments for offset" << std::endl;
    }

    lua_geti(L, offset, 1);
    lua_geti(L, offset, 2);

    attr.offset.x = (float)lua_tonumber(L, -2);
    attr.offset.y = (float)lua_tonumber(L, -1);

    lua_pop(L, 3);
  }

  if (lua_getfield(L, OPT_ARG, "textcolor") != LUA_TNIL) {
    int textcolor = lua_gettop(L);
    if (lua_rawlen(L, textcolor) != 4) {
      std::cout << "Incorrect number of arguments for textcolor" << std::endl;
    }

    lua_geti(L, textcolor, 1);
    lua_geti(L, textcolor, 2);
    lua_geti(L, textcolor, 3);
    lua_geti(L, textcolor, 4);

    attr.text_color.x = (float)lua_tonumber(L, -4);
    attr.text_color.y = (float)lua_tonumber(L, -3);
    attr.text_color.z = (float)lua_tonumber(L, -2);
    attr.text_color.w = (float)lua_tonumber(L, -1);

    // 4 for text color fields, 1 for getfield.
    lua_pop(L, 5);
  }

  if (lua_getfield(L, OPT_ARG, "textalign") != LUA_TNIL) {
    std::string align_string = lua_tostring(L, -1);
    attr.text_align = align_string_to_enum(align_string);

    lua_pop(L, 1);
  }

  if (lua_getfield(L, OPT_ARG, "textsize") != LUA_TNIL) {
    attr.text_size = (int)lua_tointeger(L, -1);
    lua_pop(L, 1);
  }

  if (lua_getfield(L, OPT_ARG, "fontname") != LUA_TNIL) {
    attr.font_name = lua_tostring(L, -1);
    lua_pop(L, 1);
  }

  GuiLuaState* state = new GuiLuaState{ L };
  qbGuiElementCallbacks_ gui_callbacks = {};

  gui_callbacks.onfocus = [](qbGuiElement el) {
    GuiLuaState* state = (GuiLuaState*)qb_guielement_getstate(el).p;
    lua_State* L = state->L;

    {
      std::lock_guard<decltype(gui_elements_mu)> l(gui_elements_mu);
      lua_rawgeti(L, LUA_REGISTRYINDEX, gui_elements[el].ref);
    }

    int element = lua_gettop(L);
    qbBool ret = QB_FALSE;
    if (lua_getfield(L, element, "onfocus") != LUA_TNIL) {
      lua_pushvalue(L, element);
      lua_call(L, 1, 1);
      ret = lua_toboolean(L, -1);
      lua_pop(L, 1);
    }
    lua_pop(L, 1);

    return ret;
  };

  gui_callbacks.onclick = [](qbGuiElement el, qbMouseButtonEvent e) {
    GuiLuaState* state = (GuiLuaState*)qb_guielement_getstate(el).p;
    lua_State* L = state->L;

    {
      std::lock_guard<decltype(gui_elements_mu)> l(gui_elements_mu);
      lua_rawgeti(L, LUA_REGISTRYINDEX, gui_elements[el].ref);
    }

    int element = lua_gettop(L);
    qbBool ret = QB_FALSE;
    if (lua_getfield(L, element, "onclick") != LUA_TNIL) {
      lua_pushvalue(L, element);
      lua_call(L, 1, 1);
      ret = lua_toboolean(L, -1);
      lua_pop(L, 1);
    }
    lua_pop(L, 1);

    return ret;
  };

  gui_callbacks.onscroll = [](qbGuiElement el, qbMouseScrollEvent e) {
    GuiLuaState* state = (GuiLuaState*)qb_guielement_getstate(el).p;
    lua_State* L = state->L;

    {
      std::lock_guard<decltype(gui_elements_mu)> l(gui_elements_mu);
      lua_rawgeti(L, LUA_REGISTRYINDEX, gui_elements[el].ref);
    }

    int element = lua_gettop(L);
    qbBool ret = QB_FALSE;
    if (lua_getfield(L, element, "onscroll") != LUA_TNIL) {
      lua_pushvalue(L, element);
      lua_call(L, 1, 1);
      ret = lua_toboolean(L, -1);
      lua_pop(L, 1);
    }
    lua_pop(L, 1);

    return ret;
  };

  gui_callbacks.onmove = [](qbGuiElement el, qbMouseMotionEvent e, int start_x, int start_y) {
    GuiLuaState* state = (GuiLuaState*)qb_guielement_getstate(el).p;
    lua_State* L = state->L;

    {
      std::lock_guard<decltype(gui_elements_mu)> l(gui_elements_mu);
      lua_rawgeti(L, LUA_REGISTRYINDEX, gui_elements[el].ref);
    }

    int element = lua_gettop(L);
    qbBool ret = QB_FALSE;
    if (lua_getfield(L, element, "onmove") != LUA_TNIL) {
      lua_pushvalue(L, element);
      lua_pushinteger(L, start_x);
      lua_pushinteger(L, start_y);
      lua_call(L, 3, 1);
      ret = lua_toboolean(L, -1);
      lua_pop(L, 1);
    }
    lua_pop(L, 1);

    return ret;
  };

  gui_callbacks.onkey = [](qbGuiElement el, qbKeyEvent e) {
    GuiLuaState* state = (GuiLuaState*)qb_guielement_getstate(el).p;
    lua_State* L = state->L;

    {
      std::lock_guard<decltype(gui_elements_mu)> l(gui_elements_mu);
      lua_rawgeti(L, LUA_REGISTRYINDEX, gui_elements[el].ref);
    }

    int element = lua_gettop(L);
    qbBool ret = QB_FALSE;
    if (lua_getfield(L, element, "onkey") != LUA_TNIL) {
      lua_pushvalue(L, element);
      lua_call(L, 1, 1);
      ret = lua_toboolean(L, -1);
      lua_pop(L, 1);
    }
    lua_pop(L, 1);

    return ret;
  };

  gui_callbacks.onrender = [](qbGuiElement el) {
    GuiLuaState* state = (GuiLuaState*)qb_guielement_getstate(el).p;
    lua_State* L = state->L;

    {
      std::lock_guard<decltype(gui_elements_mu)> l(gui_elements_mu);
      lua_rawgeti(L, LUA_REGISTRYINDEX, gui_elements[el].ref);
    }

    int element = lua_gettop(L);
    if (lua_getfield(L, element, "onrender") != LUA_TNIL) {
      lua_pushvalue(L, element);
      lua_call(L, 1, 0);
    }
    lua_pop(L, 1);
  };

  gui_callbacks.onopen = [](qbGuiElement el) {
    GuiLuaState* state = (GuiLuaState*)qb_guielement_getstate(el).p;
    lua_State* L = state->L;

    {
      std::lock_guard<decltype(gui_elements_mu)> l(gui_elements_mu);
      auto it = gui_elements.find(el);
      if (it == gui_elements.end()) {
        lua_createtable(L, 0, 0);
        int newtable = lua_gettop(L);

        lua_getglobal(L, "qb");
        lua_getfield(L, -1, "gui");
        lua_getfield(L, -1, "Element");
        lua_setmetatable(L, newtable);
        lua_pop(L, 2);

        lua_pushlightuserdata(L, el);
        lua_setfield(L, newtable, "_element");
        lua_pushvalue(L, newtable);
        gui_elements[el] = { L, luaL_ref(L, LUA_REGISTRYINDEX) };
      } else {
        lua_rawgeti(L, LUA_REGISTRYINDEX, it->second.ref);
      }      
    }

    int element = lua_gettop(L);
    if (lua_getfield(L, element, "onopen") != LUA_TNIL) {
      lua_pushvalue(L, element);
      lua_call(L, 1, 0);
    }
    lua_pop(L, 1);
  };

  gui_callbacks.onclose = [](qbGuiElement el) {
    GuiLuaState* state = (GuiLuaState*)qb_guielement_getstate(el).p;
    lua_State* L = state->L;

    {
      std::lock_guard<decltype(gui_elements_mu)> l(gui_elements_mu);
      lua_rawgeti(L, LUA_REGISTRYINDEX, gui_elements[el].ref);
    }

    int element = lua_gettop(L);
    if (lua_getfield(L, element, "onclose") != LUA_TNIL) {
      lua_pushvalue(L, element);
      lua_call(L, 1, 0);
    }
    lua_pop(L, 1);
  };

  gui_callbacks.ondestroy = [](qbGuiElement el) {
    GuiLuaState* state = (GuiLuaState*)qb_guielement_getstate(el).p;
    lua_State* L = state->L;

    int r;
    {
      std::lock_guard<decltype(gui_elements_mu)> l(gui_elements_mu);
      r = gui_elements[el].ref;
      lua_rawgeti(L, LUA_REGISTRYINDEX, r);
      gui_elements.erase(el);
    }

    int element = lua_gettop(L);
    if (lua_getfield(L, element, "ondestroy") != LUA_TNIL) {
      lua_pushvalue(L, element);
      lua_call(L, 1, 0);
    }
    luaL_unref(L, LUA_REGISTRYINDEX, r);
    lua_pop(L, 1);

    delete state;
  };

  gui_callbacks.onvaluechange = [](qbGuiElement el, qbVar old_val, qbVar new_val) {
    GuiLuaState* state = (GuiLuaState*)qb_guielement_getstate(el).p;
    lua_State* L = state->L;

    {
      std::lock_guard<decltype(gui_elements_mu)> l(gui_elements_mu);
      lua_rawgeti(L, LUA_REGISTRYINDEX, gui_elements[el].ref);
    }

    int element = lua_gettop(L);
    if (lua_getfield(L, element, "onvaluechange") != LUA_TNIL) {
      lua_pushvalue(L, element);
      push_var_to_lua(L, old_val);
      push_var_to_lua(L, new_val);
      lua_call(L, 3, 0);
    }
    lua_pop(L, 1);
  };

  gui_callbacks.onsetvalue = [](qbGuiElement el, qbVar cur_val, qbVar new_val) {
    GuiLuaState* state = (GuiLuaState*)qb_guielement_getstate(el).p;
    lua_State* L = state->L;

    {
      std::lock_guard<decltype(gui_elements_mu)> l(gui_elements_mu);
      lua_rawgeti(L, LUA_REGISTRYINDEX, gui_elements[el].ref);
    }

    int element = lua_gettop(L);
    qbVar ret = new_val;
    if (lua_getfield(L, element, "onsetvalue") != LUA_TNIL) {
      lua_pushvalue(L, element);
      push_var_to_lua(L, cur_val);
      push_var_to_lua(L, new_val);
      lua_call(L, 3, 1);
      int idx = lua_gettop(L);
      qbVar ret_val = lua_idx_to_var(L, idx);
      qb_var_destroy(&new_val);
      qb_var_destroy(&cur_val);
      ret = ret_val;

      lua_pop(L, 1);
    }
    lua_pop(L, 1);

    return ret;
  };

  gui_callbacks.ongetvalue = [](qbGuiElement el, qbVar val) {
    GuiLuaState* state = (GuiLuaState*)qb_guielement_getstate(el).p;
    lua_State* L = state->L;

    {
      std::lock_guard<decltype(gui_elements_mu)> l(gui_elements_mu);
      lua_rawgeti(L, LUA_REGISTRYINDEX, gui_elements[el].ref);
    }

    int element = lua_gettop(L);
    qbVar ret = val;
    if (lua_getfield(L, element, "ongetvalue") != LUA_TNIL) {
      lua_pushvalue(L, element);
      push_var_to_lua(L, val);
      lua_call(L, 2, 1);
      int idx = lua_gettop(L);
      qbVar ret_val = lua_idx_to_var(L, idx);
      qb_var_destroy(&ret);
      ret = ret_val;

      lua_pop(L, 1);
    }
    lua_pop(L, 1);

    return ret;
  };

  attr.callbacks = &gui_callbacks;
  attr.state = qbPtr(state);
  qbGuiElement el;  
  qb_guielement_create(&el, id, &attr);

  return 1;
}

int guielement_destroy(lua_State* L) {
  qbGuiElement el = (qbGuiElement)lua_touserdata(L, 1);
  qb_guielement_destroy(&el);
  return 0;
}

int guielement_open(lua_State* L) {
  qbGuiElement el = (qbGuiElement)lua_touserdata(L, 1);
  qb_guielement_open(el);
  return 0;
}

int guielement_close(lua_State* L) {
  qbGuiElement el = (qbGuiElement)lua_touserdata(L, 1);
  qb_guielement_close(el);
  return 0;
}

int guielement_closechildren(lua_State* L) {
  qbGuiElement el = (qbGuiElement)lua_touserdata(L, 1);
  qb_guielement_closechildren(el);
  return 0;
}

int guielement_setconstraint(lua_State* L) {
  qbGuiElement el = (qbGuiElement)lua_touserdata(L, 1);
  if (lua_type(L, 2) == LUA_TSTRING) {
    std::string property = lua_tostring(L, 2);
    std::string constraint = lua_tostring(L, 3);
    float value = (float)lua_tonumber(L, 4);
    qb_guielement_setconstraint(el,
                                property_string_to_enum(property),
                                constraint_string_to_enum(constraint),
                                value);
  } else {
    int num_args = lua_gettop(L) - 1;
    for (int i = 0; i < num_args; ++i) {
      int constraint_idx = 2 + i;
      
      lua_rawgeti(L, constraint_idx, 1);
      lua_rawgeti(L, constraint_idx, 2);
      lua_rawgeti(L, constraint_idx, 3);

      std::string property = lua_tostring(L, -3);
      std::string constraint = lua_tostring(L, -2);
      float value = (float)lua_tonumber(L, -1);
      qb_guielement_setconstraint(el,
                                  property_string_to_enum(property),
                                  constraint_string_to_enum(constraint),
                                  value);

      // 3 for property, constraint, value.
      lua_pop(L, 3);
    }
  }
  return 0;
}

int guielement_getconstraint(lua_State* L) {
  qbGuiElement el = (qbGuiElement)lua_touserdata(L, 1);
  std::string property = lua_tostring(L, 2);
  std::string constraint = lua_tostring(L, 3);
  lua_pushnumber(L, qb_guielement_getconstraint(el,
                                                property_string_to_enum(property),
                                                constraint_string_to_enum(constraint)));
  return 1;
}

int guielement_clearconstraint(lua_State* L) {
  qbGuiElement el = (qbGuiElement)lua_touserdata(L, 1);
  if (lua_type(L, 2) == LUA_TSTRING) {
    std::string property = lua_tostring(L, 2);
    std::string constraint = lua_tostring(L, 3);
    qb_guielement_clearconstraint(el,
                                  property_string_to_enum(property),
                                  constraint_string_to_enum(constraint));
  } else {
    int num_args = lua_gettop(L) - 1;
    for (int i = 0; i < num_args; ++i) {
      int constraint_idx = 2 + i;

      lua_rawgeti(L, constraint_idx, 1);
      lua_rawgeti(L, constraint_idx, 2);

      std::string property = lua_tostring(L, -2);
      std::string constraint = lua_tostring(L, -1);
      qb_guielement_clearconstraint(el,
                                    property_string_to_enum(property),
                                    constraint_string_to_enum(constraint));

      // 2 for property, constraint.
      lua_pop(L, 2);
    }
  }
  return 0;
}

int guielement_link(lua_State* L) {
  qbGuiElement el = (qbGuiElement)lua_touserdata(L, 1);
  qbGuiElement parent = (qbGuiElement)lua_touserdata(L, 2);
  qb_guielement_link(parent, el);
  return 0;
}

int guielement_unlink(lua_State* L) {
  qbGuiElement el = (qbGuiElement)lua_touserdata(L, 1);
  qb_guielement_unlink(el);
  return 0;
}

int guielement_getparent(lua_State* L) {
  qbGuiElement el = (qbGuiElement)lua_touserdata(L, 1);
  {
    std::lock_guard<decltype(gui_elements_mu)> l(gui_elements_mu);
    lua_rawgeti(L, LUA_REGISTRYINDEX, gui_elements[el].ref);
  }
  return 1;
}

int guielement_movetofront(lua_State* L) {
  qbGuiElement el = (qbGuiElement)lua_touserdata(L, 1);
  qb_guielement_movetofront(el);
  return 0;
}

int guielement_movetoback(lua_State* L) {
  qbGuiElement el = (qbGuiElement)lua_touserdata(L, 1);
  qb_guielement_movetoback(el);
  return 0;
}

int guielement_moveforward(lua_State* L) {
  qbGuiElement el = (qbGuiElement)lua_touserdata(L, 1);
  qb_guielement_moveforward(el);
  return 0;
}

int guielement_movebackward(lua_State* L) {
  qbGuiElement el = (qbGuiElement)lua_touserdata(L, 1);
  qb_guielement_movebackward(el);
  return 0;
}

int guielement_moveto(lua_State* L) {
  qbGuiElement el = (qbGuiElement)lua_touserdata(L, 1);
  float x = (float)lua_tonumber(L, 2);
  float y = (float)lua_tonumber(L, 3);

  qb_guielement_moveto(el, { x, y });
  return 0;
}

int guielement_moveby(lua_State* L) {
  qbGuiElement el = (qbGuiElement)lua_touserdata(L, 1);
  float x = (float)lua_tonumber(L, 2);
  float y = (float)lua_tonumber(L, 3);

  qb_guielement_moveby(el, { x, y });
  return 0;
}

int guielement_resizeto(lua_State* L) {
  qbGuiElement el = (qbGuiElement)lua_touserdata(L, 1);
  float x = (float)lua_tonumber(L, 2);
  float y = (float)lua_tonumber(L, 3);

  qb_guielement_resizeto(el, { x, y });
  return 0;
}

int guielement_resizeby(lua_State* L) {
  qbGuiElement el = (qbGuiElement)lua_touserdata(L, 1);
  float x = (float)lua_tonumber(L, 2);
  float y = (float)lua_tonumber(L, 3);

  qb_guielement_resizeby(el, { x, y });
  return 0;
}

int guielement_getsize(lua_State* L) {
  qbGuiElement el = (qbGuiElement)lua_touserdata(L, 1);
  vec2s size = qb_guielement_getsize(el);
  lua_pushnumber(L, size.x);
  lua_pushnumber(L, size.y);
  return 2;
}

int guielement_getposition(lua_State* L) {
  qbGuiElement el = (qbGuiElement)lua_touserdata(L, 1);
  vec2s pos = qb_guielement_getpos(el);
  lua_pushnumber(L, pos.x);
  lua_pushnumber(L, pos.y);
  return 2;
}

int guielement_setvalue(lua_State* L) {
  qbGuiElement el = (qbGuiElement)lua_touserdata(L, 1);
  qbVar var = lua_idx_to_var(L, 2);

  qb_guielement_setvalue(el, var);
  return 0;
}

int guielement_getvalue(lua_State* L) {
  qbGuiElement el = (qbGuiElement)lua_touserdata(L, 1);
  qbVar var = qb_guielement_getvalue(el);

  push_var_to_lua(L, var);

  return 1;
}

int guielement_gettext(lua_State* L) {
  qbGuiElement el = (qbGuiElement)lua_touserdata(L, 1);
  
  lua_pushstring(L, qb_guielement_gettext(el));

  return 1;
}

int guielement_settext(lua_State* L) {
  qbGuiElement el = (qbGuiElement)lua_touserdata(L, 1);
  const utf8_t* s = lua_tostring(L, 2);
  qb_guielement_settext(el, s);
  return 0;
}

int guielement_settextcolor(lua_State* L) {
  qbGuiElement el = (qbGuiElement)lua_touserdata(L, 1);

  int textcolor = lua_gettop(L);
  if (lua_rawlen(L, textcolor) != 4) {
    std::cout << "Incorrect number of arguments for textcolor" << std::endl;
  }

  lua_geti(L, textcolor, 1);
  lua_geti(L, textcolor, 2);
  lua_geti(L, textcolor, 3);
  lua_geti(L, textcolor, 4);

  vec4s text_color = {};
  text_color.x = (float)lua_tonumber(L, -4);
  text_color.y = (float)lua_tonumber(L, -3);
  text_color.z = (float)lua_tonumber(L, -2);
  text_color.w = (float)lua_tonumber(L, -1);

  // 4 for text color fields.
  lua_pop(L, 4);

  qb_guielement_settextcolor(el, text_color);

  return 0;
}

int guielement_settextscale(lua_State* L) {
  qbGuiElement el = (qbGuiElement)lua_touserdata(L, 1);
  
  int textscale = lua_gettop(L);
  if (lua_rawlen(L, textscale) != 2) {
    std::cout << "Incorrect number of arguments for textscale" << std::endl;
  }

  lua_geti(L, textscale, 1);
  lua_geti(L, textscale, 2);

  vec2s text_scale = {};
  text_scale.x = (float)lua_tonumber(L, -2);
  text_scale.y = (float)lua_tonumber(L, -1);

  // 2 for text scale fields.
  lua_pop(L, 2);

  qb_guielement_settextscale(el, text_scale);

  return 0;
}

int guielement_settextsize(lua_State* L) {
  qbGuiElement el = (qbGuiElement)lua_touserdata(L, 1);
  qb_guielement_settextsize(el, (uint32_t)lua_tointeger(L, 2));
  return 0;
}

int guielement_settextalign(lua_State* L) {
  qbGuiElement el = (qbGuiElement)lua_touserdata(L, 1);
  std::string align_string = lua_tostring(L, 2);
  qb_guielement_settextalign(el, align_string_to_enum(align_string));
  return 0;

}