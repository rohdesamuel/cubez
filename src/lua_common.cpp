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

#include "defs.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <cctype>

qbTag string_to_tag(const std::string& s) {
  if (s == "number") {
    return  QB_TAG_DOUBLE;
  } else if (s == "boolean") {
    return (qbTag)QB_TAG_LUA_BOOLEAN;
  } else if (s == "string") {
    return QB_TAG_STRING;
  } else if (s == "any") {
    return QB_TAG_ANY;
  } else if (s == "map") {
    return QB_TAG_MAP;
  } else if (s == "array") {
    return QB_TAG_ARRAY;
  }
  return QB_TAG_NIL;
}

qbSchemaFieldType_ parse_type_string(const std::string& type_string) {
  std::string s = type_string;
  std::remove_if(s.begin(), s.end(), std::isspace);

  qbSchema maybe_found;

  qbSchemaFieldType_ ret{};

  if (s.rfind("bytes", 0) == 0) {
    size_t begin = s.find_first_of('[', 0) + 1;
    size_t end = s.find_first_of(']', 0);
    if (begin == std::string::npos || end == std::string::npos || end < begin) {
      std::cout << "bad format. Expected `bytes[size]`.\n";
    }
    ret.tag = QB_TAG_BYTES;
    ret.subtag.buffer_size = std::stoi(s.substr(begin, end - begin));
  } else if (s.rfind("array") == 0) {
    size_t begin = s.find_first_of('[', 0) + 1;
    size_t end = s.find_first_of(']', 0);
    if (begin == std::string::npos || end == std::string::npos || end < begin) {
      std::cout << "bad format. Expected `bytes[size]`.\n";
    }
    auto array_type = string_to_tag(s.substr(begin, end - begin));

    ret.tag = QB_TAG_ARRAY;
    ret.subtag.array = array_type;
  } else if (s.rfind("map") == 0) {
    size_t begin = s.find_first_of('[', 0) + 1;
    size_t split = s.find_first_of(',', 0);
    size_t end = s.find_first_of(']', 0);
    if (begin == std::string::npos ||
        end == std::string::npos ||
        split == std::string::npos ||
        end < split ||
        split < begin ||
        end < begin) {
      std::cout << "bad format. Expected `map[number|string|boolean,]`.\n";
    }

    auto k_type = string_to_tag(s.substr(begin, split - begin));
    auto v_type = string_to_tag(s.substr(split + 1, end - split - 1));

    ret.tag = QB_TAG_MAP;
    ret.subtag.map.k = k_type;
    ret.subtag.map.v = v_type;
  } else if (qb_component_find(s.data(), &maybe_found) >= 0) {
    assert(false && "unimplemented");
  } else {
    ret.tag = string_to_tag(s);
  }
  return ret;
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

    case QB_TAG_CSTRING:
    case QB_TAG_STRING:
      lua_pushstring(L, v.s);
      break;

    case QB_TAG_BYTES:
      lua_pushlstring(L, (const char*)v.p, v.size);
      break;

    case QB_TAG_PTR:
      lua_pushlightuserdata(L, v.p);
      break;

    case QB_TAG_MAP:
    {
      qbVar* map = (qbVar*)lua_newuserdata(L, sizeof(qbVar));
      *map = v;
      int userdata = lua_gettop(L);

      lua_getglobal(L, "qb");
        lua_getfield(L, -1, "Map");
        lua_setmetatable(L, userdata);
      lua_pop(L, 1);
      break;
    }

    case QB_TAG_ARRAY:
    {
      qbVar* map = (qbVar*)lua_newuserdata(L, sizeof(qbVar));
      *map = v;
      int userdata = lua_gettop(L);

      lua_getglobal(L, "qb");
        lua_getfield(L, -1, "Array");
        lua_setmetatable(L, userdata);
      lua_pop(L, 1);
      break;
    }
  }
}

qbVar copy_table(lua_State* L, int idx) {
  if (lua_type(L, idx) != LUA_TTABLE) {
    return lua_idx_to_var(L, idx);
  }

  qbVar ret = qbMap(QB_TAG_ANY, QB_TAG_ANY);

  
  lua_pushnil(L);  /* first key */
  while (lua_next(L, idx) != 0) {
    qbVar key = lua_idx_to_var(L, -2);
    qbVar val = copy_table(L, lua_gettop(L));
    qb_map_insert(ret, key, val);

    /* removes 'value'; keeps 'key' for next iteration */
    lua_pop(L, 1);
  }

  return ret;
}

qbVar lua_idx_to_var(lua_State* L, int idx) {
  if (idx == -1) {
    idx = lua_gettop(L);
  }

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
      const char* str = lua_tolstring(L, idx, &len);
      return qbString(str);
    }

    case LUA_TTABLE:
      return copy_table(L, idx);
  }

  return qbNil;
}