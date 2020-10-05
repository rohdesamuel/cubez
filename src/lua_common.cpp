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

#include "lua_common.h"

#include <iostream>
#include <string>

std::pair<qbTag, size_t> parse_type_string(const std::string& s) {
  if (s == "number") {
    return{ QB_TAG_DOUBLE, sizeof(double) };
  } else if (s == "boolean") {
    return{ (qbTag)QB_TAG_LUA_BOOLEAN, sizeof(int) };
  } else if (s.rfind("string", 0) == 0) {
    return{ QB_TAG_STRING, sizeof(qbStr*) };
  } else if (s.rfind("bytes", 0) == 0) {
    size_t begin = s.find_first_of('[', 0) + 1;
    size_t end = s.find_first_of(']', 0);
    if (begin == std::string::npos || end == std::string::npos || end < begin) {
      std::cout << "bad format. Expected `bytes[size]`.\n";
    }
    return{ QB_TAG_BYTES, std::stoi(s.substr(begin, end - begin)) };
  } else if (s == "function") {
    return{ (qbTag)QB_TAG_LUA_FUNCTION, sizeof(qbLuaReference) };
  } else if (s == "table") {
    return{ (qbTag)QB_TAG_LUA_TABLE, sizeof(qbLuaReference) };
  }
  return{ QB_TAG_NIL, 0 };
}

void push_new_table(lua_State*L, const char* metatable) {
  // Create the return value.
  lua_newtable(L);

  // Get the qb.Component metatable.
  lua_getglobal(L, "qb");
  lua_getfield(L, -1, metatable);

  // Set the return's metatable.
  lua_setmetatable(L, -3);
  lua_pop(L, 1);
}

void push_var_to_lua(lua_State* L, qbVar v) {
  switch (v.tag) {
    case QB_TAG_NIL:
      lua_pushnil(L);
      break;

    case QB_TAG_LUA_BOOLEAN:
      lua_pushboolean(L, v.i);
      break;

    case QB_TAG_DOUBLE:
      lua_pushnumber(L, v.d);
      break;

    case QB_TAG_INT:
    case QB_TAG_UINT:
      lua_pushinteger(L, v.i);
      break;

    case QB_TAG_STRING:
      lua_pushstring(L, v.s->bytes);
      break;

    case QB_TAG_BYTES:
      lua_pushlstring(L, (const char*)v.p, v.size);
      break;

    case QB_TAG_PTR:
      lua_pushlightuserdata(L, v.p);
      break;
  }
}

void push_var_to_lua(lua_State* L, qbRef r) {
  switch (r.tag) {
    case QB_TAG_NIL:
      lua_pushnil(L);
      break;

    case QB_TAG_LUA_BOOLEAN:
      lua_pushboolean(L, *r.i);
      break;

    case QB_TAG_DOUBLE:
      lua_pushnumber(L, *r.d);
      break;

    case QB_TAG_INT:
    case QB_TAG_UINT:
      lua_pushinteger(L, *r.i);
      break;

    case QB_TAG_STRING:
      lua_pushstring(L, (*r.s)->bytes);
      break;

    case QB_TAG_BYTES:
      lua_pushlstring(L, (const char*)r.p, r.size);
      break;

    case QB_TAG_PTR:
      lua_pushlightuserdata(L, r.p);
      break;
  }
}

qbVar lua_idx_to_var(lua_State* L, int idx) {
  int type = lua_type(L, idx);
  switch (type) {
    case LUA_TBOOLEAN:
      return qbInt(lua_toboolean(L, idx));

    case LUA_TLIGHTUSERDATA:
      return qbPtr(lua_touserdata(L, idx));

    case LUA_TNUMBER:
    {
      double d = lua_tonumber(L, idx);
      if ((int)d == d) {
        return qbDouble(d);
      }
      return qbInt(lua_tointeger(L, idx));
    }

    case LUA_TSTRING:
    {
      size_t len;
      const char* str = lua_tolstring(L, -1, &len);
      return qbString(str);
    }
  }

  return qbNil();
}