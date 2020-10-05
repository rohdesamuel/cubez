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

#include "lua_cubez_bindings.h"
#include "lua_common.h"

#include <cubez/cubez.h>

#include "component_registry.h"

extern "C" {
#include <lua/lua.h>
#include <lua/lualib.h>
#include <lua/lauxlib.h>
}

namespace
{

void _entity_add_component(lua_State* L, qbEntity entity, qbComponent component, int schema_pos) {
  qbSchema schema = qb_component_schema(component);
  qbStruct_* struct_inst = (qbStruct_*)alloca(sizeof(qbStructInternals_) + schema->size);
  memset(struct_inst, 0, sizeof(qbStructInternals_) + schema->size);
  struct_inst->internals.schema = schema;

  char* entity_buf = &struct_inst->data;

  lua_table_iterate(L, schema_pos, [&](lua_State* L) -> bool {
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
      case QB_TAG_INT:
      case QB_TAG_UINT:
      {
        double n = lua_tonumber(L, -1);
        memcpy(entity_buf + offset, &n, size);
        break;
      }

      case QB_TAG_STRING:
      {
        size_t len;
        const char* lua_str = lua_tolstring(L, -1, &len);
        qbStr* str = (qbStr*)malloc(sizeof(qbStr) + len + 1);
        str->len = len;
        STRCPY(str->bytes, len + 1, lua_str);

        memcpy(entity_buf + offset, &str, sizeof(qbStr*));
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

  qb_entity_addcomponent(entity, component, struct_inst);
}

void _entity_add_components(lua_State* L, qbEntity entity, int schema_pos) {
  lua_table_iterate(L, schema_pos, [entity](lua_State* L) -> bool {
    if (!lua_isstring(L, -2) || !lua_istable(L, -1)) {
      std::string key_name = lua_typename(L, lua_type(L, -2));
      std::string value_name = lua_typename(L, lua_type(L, -1));
      std::cout << "Expected a table of KV pairs of "
        << "[string, table]. Given args: key: "
        << key_name.c_str() << ", value: "
        << value_name.c_str() << std::endl;
      return true;
    }

    lua_getfield(L, -1, "id");
    qbComponent component = lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "_data");

    _entity_add_component(L, entity, component, lua_gettop(L));

    // Pop _data off of stack.
    lua_pop(L, 1);

    return false;
  });
}

}

int component_create(lua_State* L) {
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

  lua_table_iterate(L, COMPONENT_SCHEMA_ARG, [&](lua_State* L) -> bool {
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

  // Return a new table: { id = new component } with qb.Component as the
  // metatable.
  push_new_table(L, "Component");

  // Set the return's component.
  lua_pushinteger(L, new_component);
  lua_setfield(L, -2, "id");

  return 1;
}

int component_find(lua_State* L) {
  const char* name = lua_tostring(L, 1);
  lua_pushinteger(L, qb_component_find(name, nullptr));
  return 1;
}

int component_oncreate(lua_State* L) {
  qbComponent component = lua_tointeger(L, 1);
  lua_pushvalue(L, 2);
  int fn_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_newtable(L);
  lua_pushinteger(L, 0);
  lua_setfield(L, -2, "id");
  int arg_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  struct ComponentOnCreateState {
    int fn_ref;
    int arg_ref;
    lua_State* L;
  };

  ComponentOnCreateState* state = new ComponentOnCreateState();
  state->fn_ref = fn_ref;
  state->arg_ref = arg_ref;
  state->L = L;

  qbVar v = qbPtr(state);
  qb_instance_oncreate(component, [](qbInstance instance, qbVar state) {
    ComponentOnCreateState* create_state = (ComponentOnCreateState*)state.p;
    lua_rawgeti(create_state->L, LUA_REGISTRYINDEX, create_state->fn_ref);
    lua_rawgeti(create_state->L, LUA_REGISTRYINDEX, create_state->arg_ref);
    lua_pushinteger(create_state->L, instance->entity);
    lua_setfield(create_state->L, -2, "id");    
    lua_call(create_state->L, 1, 0);
  }, v);

  return 0;
}

int component_ondestroy(lua_State* L) {
  return 0;
}

int entity_create(lua_State* L) {
  static const int COMPONENT_LIST_ARG = 1;

  qbEntity entity;
  {
    qbEntityAttr attr;
    qb_entityattr_create(&attr);
    qb_entity_create(&entity, attr);
    qb_entityattr_destroy(&attr);
  }

  int num_args = lua_gettop(L);
  for (int i = 0; i < num_args; ++i) {
    int data_idx = 1 + i;
    lua_getfield(L, data_idx, "id");
    qbComponent component = lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, data_idx, "_data");
    _entity_add_component(L, entity, component, lua_gettop(L));
    lua_pop(L, 1);
  }

  lua_pushinteger(L, entity);
  return 1;
}

int entity_destroy(lua_State* L) {
  qbEntity e = lua_tointeger(L, 1);
  qb_entity_destroy(e);
  return 0;
}

int entity_addcomponent(lua_State* L) {
  static const int ENTITY_ARG = 1;
  static const int COMPONENT_ARG = 2;

  qbEntity entity = lua_tointeger(L, ENTITY_ARG);

  int num_args = lua_gettop(L) - 1;
  for (int i = 0; i < num_args; ++i) {
    int data_idx = 2 + i;
    lua_getfield(L, data_idx, "id");
    qbComponent component = lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, data_idx, "_data");
    _entity_add_component(L, entity, component, lua_gettop(L));
    lua_pop(L, 1);
  }

  return 0;
}

int entity_removecomponent(lua_State* L) {
  qbEntity e = lua_tointeger(L, 1);
  qbComponent c = lua_tointeger(L, 2);
  qb_entity_removecomponent(e, c);
  return 0;
}

int entity_hascomponent(lua_State* L) {
  qbEntity e = lua_tointeger(L, 1);
  qbComponent c = lua_tointeger(L, 2);
  lua_pushboolean(L, qb_entity_hascomponent(e, c));
  return 1;
}

int system_create(lua_State* L) {
  qbLuaFunction* fn = new qbLuaFunction();
  lua_pushvalue(L, 2);
  fn->function_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  push_new_table(L, "Entity");
  lua_pushinteger(L, 0);
  lua_setfield(L, -2, "id");
  fn->entity_arg_ref = luaL_ref(L, LUA_REGISTRYINDEX);
  
  fn->l = L;

  lua_table_iterate(L, 1, [fn](lua_State* L) {
    lua_getfield(L, -1, "id");
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
      lua_rawgeti(f->l, LUA_REGISTRYINDEX, f->function_ref);
      lua_rawgeti(f->l, LUA_REGISTRYINDEX, f->entity_arg_ref);
      lua_pushinteger(f->l, qb_instance_entity(insts[0]));
      lua_setfield(f->l, -2, "id");

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
            {
              double d = *(double*)(buf + field.offset);
              lua_pushnumber(f->l, d);
              break;
            }

            case QB_TAG_INT:
            case QB_TAG_UINT:
            {
              int64_t i = *(int64_t*)(buf + field.offset);
              lua_pushinteger(f->l, i);
              break;
            }

            case QB_TAG_STRING:
            {
              qbStr* s = *(qbStr**)(buf + field.offset);
              lua_pushstring(f->l, s->bytes);
              break;
            }

            case QB_TAG_BYTES:
            {
              const char* s = (const char*)(buf + field.offset);
              lua_pushlstring(f->l, s, field.size);
              break;
            }

            case QB_TAG_LUA_TABLE:
            case QB_TAG_LUA_FUNCTION:
            {
              qbLuaReference* lua_ref = (qbLuaReference*)(buf + field.offset);
              lua_rawgeti(f->l, LUA_REGISTRYINDEX, lua_ref->ref);
            }
          }
          lua_setfield(f->l, -2, field.key.c_str());
        }
      }

      lua_call(f->l, (int)f->components.size() + 1, 0);

      for (size_t i = 0; i < f->components.size(); ++i) {
        qbSchema schema = qb_component_schema(f->components[i]);

        char* buf;
        qb_instance_const(insts[i], &buf);

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
            {
              double d = lua_tonumber(f->l, -1);
              *(double*)(buf + field.offset) = d;
              break;
            }

            case QB_TAG_INT:
            case QB_TAG_UINT:
            {
              int64_t i = lua_tointeger(f->l, -1);
              *(int64_t*)(buf + field.offset) = i;
              break;
            }

            case QB_TAG_STRING:
            {
              free(*(qbStr**)(buf + field.offset));
              size_t len;
              const char* lua_str = lua_tolstring(f->l, -1, &len);
              qbStr* str = (qbStr*)malloc(sizeof(qbStr) + len + 1);
              str->len = len;
              STRCPY(str->bytes, len + 1, lua_str);
              
              *(qbStr**)(buf + field.offset) = str;
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

int component_count(lua_State* L) {
  qbComponent component = lua_tointeger(L, 1);
  lua_pushinteger(L, qb_component_count(component));
  return 1;
}

int entity_getcomponent(lua_State* L) {
  qbEntity entity = lua_tointeger(L, 1);
  qbComponent component = lua_tointeger(L, 2);

  if (!qb_entity_hascomponent(entity, component)) {
    lua_pushnil(L);
    return 1;
  }

  qbStruct_* buf;
  qb_instance_find(component, entity, &buf);

  qbStruct_** s = (qbStruct_**)lua_newuserdata(L, sizeof(qbStruct_*));
  *s = buf;

  int userdata = lua_gettop(L);
  
  lua_getglobal(L, "qb");
  lua_getfield(L, -1, "Instance");
  lua_setmetatable(L, userdata);
  lua_pop(L, 1);

  return 1;
}

int instance_get(lua_State* L) {
  if (!lua_isuserdata(L, 1)) {
    lua_pushnil(L);
    return 1;
  }

  qbStruct_* s = *(qbStruct_**)lua_touserdata(L, 1);
  qbVar v = qbStruct(s->internals.schema, s);
  const char* key = lua_tostring(L, 2);

  qbRef r = qb_ref_at(v, key);
  push_var_to_lua(L, r);
  
  return 1;
}

int instance_set(lua_State* L) {
  if (!lua_isuserdata(L, 1)) {
    return 0;
  }

  qbStruct_* s = *(qbStruct_**)lua_touserdata(L, 1);
  qbVar v = qbStruct(s->internals.schema, s);
  const char* key = lua_tostring(L, 2);
  qbVar val = lua_idx_to_var(L, 3);  
  qbRef ref = qb_ref_at(v, key);

  if (val.tag != ref.tag) {
    return 0;
  }

  switch (ref.tag) {
    case QB_TAG_LUA_BOOLEAN:
    case QB_TAG_INT:
    case QB_TAG_UINT:
      *ref.i = val.i;
      break;

    case QB_TAG_DOUBLE:
      *ref.d = val.d;
      break;

    case QB_TAG_STRING:
      free(*ref.s);
      *ref.s = qbString(val.s->bytes).s;
      break;

    case QB_TAG_BYTES:
      memcpy(ref.s, val.s, std::min(ref.size, val.size));
      break;

    case QB_TAG_PTR:
      ref.p = val.p;
      break;
  }

  return 0;
}