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

#ifndef CUBEZ_LUA_COMMON__H
#define CUBEZ_LUA_COMMON__H

#include <cubez/cubez.h>
#include <string>
#include <vector>

extern "C" {
#include <lua/lua.h>
}

struct lua_State;

#define LUA_CHECK_TYPE(L, idx, type) assert(lua_type(L, idx) == type && "Lua at " #idx " does not match type \"" #type "\"")

typedef enum {
  QB_TAG_LUA_BOOLEAN = QB_TAG_BYTES + 1,
  QB_TAG_LUA_TABLE,
  QB_TAG_LUA_FUNCTION,
} qbScriptTag;

// Map of component -> List[Tuple[Key, Type, Offset, PtrSize, BufSize]]
struct qbLuaReference {
  lua_State* l;
  int ref;
};

struct qbLuaFunction {
  int function_ref;
  int entity_arg_ref;
  std::vector<qbComponent> components;
  std::vector<int> instance_tables;

  lua_State* l;
};

template<class F>
void lua_table_iterate(lua_State* L, int i, F&& f) {
  lua_pushnil(L);  /* first key */
  while (lua_next(L, i) != 0) {
    if (f(L)) {
      break;
    }

    /* removes 'value'; keeps 'key' for next iteration */
    lua_pop(L, 1);
  }
}

struct ParsedType {
  qbTag main_type;
  
  union {
    qbTag k_type;
    qbTag v_type;
  };

  size_t size;
};

void push_new_table(lua_State* L, const char* metatable);

struct qbSchemaFieldType_ parse_type_string(const std::string& s);

void push_var_to_lua(lua_State* L, qbVar v);

qbVar lua_idx_to_var(lua_State* L, int idx);

#endif  // CUBEZ_LUA_COMMON__H