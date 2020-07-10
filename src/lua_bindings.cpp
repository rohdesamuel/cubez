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

#include <cubez/common.h>
#include <cubez/cubez.h>
#include <cubez/render.h>
#include "component_registry.h"
#include "lua_bindings.h"

extern "C" {
#include <lua/lua.h>
#include <lua/lualib.h>
#include <lua/lauxlib.h>
}

#include <string>
#include <iostream>
#include <vector>
#include <tuple>
#include <functional>
#include <algorithm>

#include <filesystem>
namespace std
{
namespace filesystem = experimental::filesystem;
}

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
  int function_reference;
  std::vector<qbComponent> components;
  std::vector<int> instance_tables;

  lua_State* l;
};

template<class F>
void for_each(lua_State* L, int i, F&& f) {
  lua_pushnil(L);  /* first key */
  while (lua_next(L, i) != 0) {
    if (f(L)) {
      break;
    }

    /* removes 'value'; keeps 'key' for next iteration */
    lua_pop(L, 1);
  }
}

std::pair<qbTag, size_t> parse_type_string(const std::string& s) {
  if (s == "number") {
    return { QB_TAG_DOUBLE, sizeof(double) };
  } else if (s == "boolean") {
    return{ (qbTag)QB_TAG_LUA_BOOLEAN, sizeof(int) };
  } else if (s.rfind("string", 0) == 0) {
    return{ QB_TAG_STRING, sizeof(char*) };
  } else if (s.rfind("bytes", 0) == 0) {
    size_t begin = s.find_first_of('[', 0) + 1;
    size_t end = s.find_first_of(']', 0);
    if (begin == std::string::npos || end == std::string::npos || end < begin) {
      std::cout << "bad format\n";
    }
    return{ QB_TAG_BYTES, std::stoi(s.substr(begin, end - begin)) };
  } else if (s == "function") {
    return{ (qbTag)QB_TAG_LUA_FUNCTION, sizeof(qbLuaReference) };
  } else if (s == "table") {
    return{ (qbTag)QB_TAG_LUA_TABLE, sizeof(qbLuaReference) };
  }
  return{ QB_TAG_NIL, 0 };
}

static int component_create(lua_State* L) {
  static const int COMPONENT_NAME_ARG = 1;
  static const int COMPONENT_SCHEMA_ARG = 2;
  static const int COMPONENT_OPTS_ARG = 3;

  if (lua_gettop(L) < 3) {
    lua_pushnil(L);
  }

  // Iterate through the table on the stack and generate a schema for the given
  // component. This allows for the dynamic construction of components instead
  // of defining a new struct.
  size_t struct_size = 0;
  qbSchema schema = new qbSchema_;

  const char* name = lua_tostring(L, COMPONENT_NAME_ARG);
  
  for_each(L, COMPONENT_SCHEMA_ARG, [&](lua_State* L) -> bool {
    if (!lua_istable(L, -1)) {
      std::cout << "expected table\n";
    }

    lua_pushnil(L);
    lua_next(L, -2);

    // Check key and value are strings.
    if (!lua_isstring(L, -2) || !lua_isstring(L, -1)) {
      std::string key_name = lua_typename(L, lua_type(L, -2));
      std::string value_name = lua_typename(L, lua_type(L, -1));
      std::cout << "component_create takes a table of KV pairs of strings. Given args: "
        << "key: " << key_name.c_str() << ", value: " << value_name.c_str() << std::endl;
      return true;
    }
    std::string key = lua_tostring(L, -2);
    std::string value = lua_tostring(L, -1);

    key.erase(std::remove_if(key.begin(), key.end(), isspace), key.end());
    value.erase(std::remove_if(value.begin(), value.end(), isspace), value.end());

    auto type_size = parse_type_string(value);
    
    schema->fields.push_back(qbSchemaField_{ key, type_size.first, struct_size, type_size.second });
    struct_size += type_size.second;

    lua_pop(L, 2);
    return false;
  });
  schema->size = struct_size;

  qbComponent new_component;
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatasize(attr, sizeof(qbStructInternals_) + struct_size);
    qb_componentattr_setschema(attr, schema);
    qb_component_create(&new_component, name, attr);
    qb_componentattr_destroy(&attr);
  }

  /*
  qb_instance_ondestroy(new_component, [](qbInstance inst) {
    qbScriptInstance* script_inst;
    qb_instance_const(inst, &script_inst);
    qbComponent c = script_inst->m.c;
    char* buf = &script_inst->data;

    for (const auto& f : schemas[c].fields) {
      
    }

    for (const auto& f : schemas[c].fields) {
      if (f.type == qbScriptType::QB_SCRIPT_TYPE_TABLE || f.type == qbScriptType::QB_SCRIPT_TYPE_FUNCTION) {
        qbLuaReference* lua_ref = (qbLuaReference*)(buf + f.offset);
        luaL_unref(lua_ref->l, LUA_REGISTRYINDEX, lua_ref->ref);
      }
    }
  });*/

  /*if (!lua_isnil(L, COMPONENT_OPTS_ARG)) {
    if (lua_getfield(L, COMPONENT_OPTS_ARG, "oncreate") != LUA_TNIL) {
      component_schema.on_create = { L, luaL_ref(L, LUA_REGISTRYINDEX) };
    } else {
      lua_pop(L, 1);
    }

    if (lua_getfield(L, COMPONENT_OPTS_ARG, "ondestroy") != LUA_TNIL) {
      component_schema.on_destroy = { L, luaL_ref(L, LUA_REGISTRYINDEX) };
    } else {
      lua_pop(L, 1);
    }
  }*/

  // Return a new table: { _size = struct_size } with qb.Component as the
  // metatable.
  // Create the return value.
  lua_newtable(L);
 
  // Get the qb.Component metatable.
  lua_getglobal(L, "qb");
  lua_getfield(L, -1, "Component");

  // Set the return's metatable.
  lua_setmetatable(L, -3);
  lua_pop(L, 1);

  // Set the return's component.
  lua_pushinteger(L, new_component);
  lua_setfield(L, -2, "_component");

  // Set the return's schema.
  lua_pushvalue(L, 1);
  lua_setfield(L, -2, "_schema");
  
  return 1;
}

static int component_find(lua_State* L) {
  const char* name = lua_tostring(L, 1);
  lua_pushinteger(L, qb_component_find(name, nullptr));
  return 1;
}

static int entity_create(lua_State* L) {
  static const int COMPONENT_LIST_ARG = 1;

  if (!lua_istable(L, COMPONENT_LIST_ARG)) {
    std::cout << "not a table\n";
  }

  qbEntity entity;
  {
    qbEntityAttr attr;
    qb_entityattr_create(&attr);
    qb_entity_create(&entity, attr);
    qb_entityattr_destroy(&attr);
  }

  for_each(L, COMPONENT_LIST_ARG, [entity](lua_State* L) -> bool {
    if (!lua_isstring(L, -2) || !lua_istable(L, -1)) {
      std::string key_name = lua_typename(L, lua_type(L, -2));
      std::string value_name = lua_typename(L, lua_type(L, -1));
      std::cout << "entity_create takes a table of KV pairs of "
        << "[string, table]. Given args: key: "
        << key_name.c_str() << ", value: "
        << value_name.c_str() << std::endl;
      return true;
    }
    
    lua_getfield(L, -1, "_component");
    qbComponent component = lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "_data");
    int top = lua_gettop(L);

    qbSchema schema = qb_component_schema(component);
    qbStruct_* struct_inst = (qbStruct_*)alloca(sizeof(qbStructInternals_) + schema->size);
    memset(struct_inst, 0, sizeof(qbStructInternals_) + schema->size);
    struct_inst->internals.schema = schema;
    struct_inst->internals.c = component;

    char* entity_buf = &struct_inst->data;
    for_each(L, top, [&](lua_State* L) -> bool {
      qbTag type = QB_TAG_NIL;
      size_t offset = 0;
      size_t size = 0;

      if (lua_isnumber(L, -2)) {
        int64_t key = lua_tointeger(L, -2);
        type = schema->fields[key - 1].tag;
        offset = schema->fields[key - 1].offset;
        size = schema->fields[key - 1].size;
      } else if (lua_isstring(L, -2)) {
        std::string key = lua_tostring(L, -2);
        for (const auto& f : schema->fields) {
          if (f.key == key) {
            type = f.tag;
            offset = f.offset;
            size = f.size;
            break;
          }
        }
      }

      switch (type) {
        case QB_TAG_NIL:
          break;

        case QB_TAG_LUA_BOOLEAN:
        {
          int b = lua_toboolean(L, -1);
          memcpy(entity_buf + offset, &b, size);
          break;
        }

        case QB_TAG_DOUBLE:
          // Fall-through intended.
        case QB_TAG_INT:
        {
          double n = lua_tonumber(L, -1);
          memcpy(entity_buf + offset, &n, size);
          break;
        }

        case QB_TAG_STRING:
        {
          size_t len;
          const char* str = lua_tolstring(L, -1, &len);
          char* copy = (char*)malloc(len);
          STRCPY(copy, len, str);
          memcpy(entity_buf + offset, &copy, sizeof(char*));
          break;
        }

        case QB_TAG_BYTES:
        {
          size_t len;
          const char* str = lua_tolstring(L, -1, &len);
          len = std::min(len, size);
          memcpy(entity_buf + offset, str, len);
          break;
        }

        case QB_TAG_LUA_TABLE:
          // Fall-through intended.
        case QB_TAG_LUA_FUNCTION:
        {
          lua_pushvalue(L, -1);
          qbLuaReference ref = { L, luaL_ref(L, LUA_REGISTRYINDEX) };
          memcpy(entity_buf + offset, &ref, sizeof(ref));
          break;
        }
      }

      return false;
    });

    // Pop _data off of stack.
    lua_pop(L, 1);
    qb_entity_addcomponent(entity, component, struct_inst);

    return false;
  });

  lua_pushinteger(L, entity);
  return 1;
}

static int entity_destroy(lua_State* L) {
  qbEntity e = lua_tointeger(L, 1);
  qb_entity_destroy(e);
  return 0;
}

static int system_create(lua_State* L) {
  qbLuaFunction* fn = new qbLuaFunction();
  lua_pushvalue(L, 2);
  fn->function_reference = luaL_ref(L, LUA_REGISTRYINDEX);
  fn->l = L;

  for_each(L, 1, [fn](lua_State* L) {
    lua_getfield(L, -1, "_component");
    qbComponent component = lua_tointeger(L, -1);
    qbSchema schema = qb_component_schema(component);

    fn->components.push_back(component);

    lua_createtable(L, (int)schema->fields.size(), (int)schema->fields.size());
    for (const auto& f : schema->fields) {
      lua_pushnil(L);
      lua_setfield(L, -2, f.key.c_str());
    }

    fn->instance_tables.push_back(luaL_ref(L, LUA_REGISTRYINDEX));

    lua_pop(L, 1);
    return false;
  });

  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    for (auto component : fn->components) {
      qb_systemattr_addconst(attr, component);
    }

    qb_systemattr_setfunction(attr, [](qbInstance* insts, qbFrame* frame) {
      qbLuaFunction* f = (qbLuaFunction*)frame->state;
      lua_rawgeti(f->l, LUA_REGISTRYINDEX, f->function_reference);

      for (size_t i = 0; i < f->components.size(); ++i) {
        qbSchema schema = qb_component_schema(f->components[i]);

        char* buf;
        qb_instance_const(insts[i], &buf);
        lua_rawgeti(f->l, LUA_REGISTRYINDEX, f->instance_tables[i]);
        for (const auto& field : schema->fields) {
          switch (field.tag) {
            case QB_TAG_LUA_BOOLEAN:
            {
              int b = *(int*)(buf + field.offset);
              lua_pushboolean(f->l, b);
              break;
            }

            case QB_TAG_DOUBLE:
              // Fall-through intended.
            case QB_TAG_INT:
            {
              double d = *(double*)(buf + field.offset);
              lua_pushnumber(f->l, d);
              break;
            }

            case QB_TAG_STRING:
            {
              const char* s = *(const char**)(buf + field.offset);
              lua_pushstring(f->l, s);
              break;
            }

            case QB_TAG_BYTES:
            {
              const char* s = (const char*)(buf + field.offset);
              lua_pushlstring(f->l, s, field.size);
              break;
            }

            case QB_TAG_LUA_TABLE:
              // Fall-through intended.
            case QB_TAG_LUA_FUNCTION:
            {
              qbLuaReference* lua_ref = (qbLuaReference*)(buf + field.offset);
              lua_rawgeti(f->l, LUA_REGISTRYINDEX, lua_ref->ref);
            }
          }
          lua_setfield(f->l, -2, field.key.c_str());
        }
      }

      lua_call(f->l, (int)f->components.size(), 0);

      for (size_t i = 0; i < f->components.size(); ++i) {
        qbSchema schema = qb_component_schema(f->components[i]);

        qbStruct_* script_inst;
        qb_instance_const(insts[i], &script_inst);
        char* buf = &script_inst->data;

        lua_rawgeti(f->l, LUA_REGISTRYINDEX, f->instance_tables[i]);
        for (const auto& field : schema->fields) {
          lua_getfield(f->l, -1, field.key.c_str());

          switch (field.tag) {
            case QB_TAG_LUA_BOOLEAN:
            {
              int b = lua_toboolean(f->l, -1);
              *(int*)(buf + field.offset) = b;
              break;
            }

            case QB_TAG_DOUBLE:
              // Fall-through intended.
            case QB_TAG_INT:
            {
              double d = lua_tonumber(f->l, -1);
              *(double*)(buf + field.offset) = d;
              break;
            }

            case QB_TAG_STRING:
            {
              free(*(char**)(buf + field.offset));
              size_t len;
              const char* str = lua_tolstring(f->l, -1, &len);
              char* copy = (char*)malloc(len);
              STRCPY(copy, len, str);
              *(char**)(buf + field.offset) = copy;
              break;
            }

            case QB_TAG_BYTES:
            {
              size_t len;
              const char* str = lua_tolstring(f->l, -1, &len);
              len = std::min(len, field.size);
              memcpy(buf + field.offset, str, len);
              break;
            }
          }
          lua_pop(f->l, 1);
        }
        lua_pop(f->l, 1);
      }
    });

    qb_systemattr_setuserstate(attr, fn);

    qbSystem system;
    qb_system_create(&system, attr);

    qb_systemattr_destroy(&attr);
  }

  lua_pushnumber(L, 1);
  return 1;
}

static int component_count(lua_State* L) {
  qbComponent component = lua_tointeger(L, 1);
  lua_pushinteger(L, qb_component_count(component));
  return 1;
}

static const luaL_Reg qb_lib[] = {
  { "component_create", component_create },
  { "component_count", component_count },
  { "component_find", component_find },

  { "entity_create", entity_create },
  { "entity_destroy", entity_destroy },

  { "system_create", system_create },
  { nullptr, nullptr }
};

const char QB_LUA_LIB[] = "qb";
const char QB_LUA_INIT[] =
R"(

-- Initialization code for the Lua portion of scripting. This is so that a file
-- doesn't have to be included with the game on how to initialize the engine.

-- The Component metatable.
local _QB = qb
_QB.Component = {}
_QB.Component.__index = _QB.Component

function _QB.Component:new (tdata)
  return { _component=self._component, _data=tdata }
end

function _QB.Component:count()
  return component_getcount(self._component)
end

)";

void lua_start(lua_State* L) {
  // Call qb.init(width, height)
  lua_getglobal(L, "qb");
  lua_getfield(L, -1, "start");
  if (!lua_isnil(L, -1)) {
    lua_pushnumber(L, qb_window_width());
    lua_pushnumber(L, qb_window_height());
    lua_call(L, 2, 0);
  }
  lua_pop(L, 1);
}

void lua_update(lua_State* L) {
  // Call qb.udpate()
  lua_getglobal(L, "qb");
  lua_getfield(L, -1, "update");
  if (!lua_isnil(L, -1)) {
    lua_call(L, 0, 0);
  }
  lua_pop(L, 1);
}

void lua_init(lua_State* L) {
  // Call qb.init()
  lua_getglobal(L, "qb");
  lua_getfield(L, -1, "init");
  if (!lua_isnil(L, -1)) {
    lua_call(L, 0, 0);
  }
  lua_pop(L, 1);
}

std::filesystem::path directory;
std::filesystem::path entrypoint;

void lua_bindings_initialize(struct qbScriptAttr_* attr) {
  if (!attr->directory) {
    directory = std::filesystem::current_path();
  } else {
    char** d = attr->directory;
    while (*d) {
      directory.append(*d);
      ++d;
    }
  }

  if (!attr->entrypoint) {
    entrypoint = std::filesystem::path(directory).append("main.lua");
  } else {
    entrypoint = std::filesystem::path(directory).append(attr->entrypoint);
  }
}

lua_State* lua_thread_initialize() {
  std::cout << "Initializing Lua\n";
  lua_State* L = luaL_newstate();

  luaL_openlibs(L);
  lua_newtable(L);
  luaL_setfuncs(L, qb_lib, 0);
  lua_setglobal(L, QB_LUA_LIB);

  if (luaL_dostring(L, QB_LUA_INIT) != LUA_OK) {
    std::cout << lua_tostring(L, -1) << std::endl;
  }

  if (luaL_dofile(L, entrypoint.generic_string().c_str()) != LUA_OK) {
    std::cout << lua_tostring(L, -1) << std::endl;
  }

  lua_init(L);

  return L;
}