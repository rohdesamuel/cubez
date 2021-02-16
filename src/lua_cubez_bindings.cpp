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
#include "cubez/struct.h"

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

  qbVar* entity_buf = (qbVar*)(&struct_inst->data);

  lua_table_iterate(L, schema_pos, [&](lua_State* L) -> bool {
    qbSchemaFieldType_ type{};
    size_t index = 0;

    if (lua_isnumber(L, -2)) {
      int64_t key = lua_tointeger(L, -2);
      type = schema->fields[key - 1].type;
      index = key - 1;
    } else if (lua_isstring(L, -2)) {
      std::string key = lua_tostring(L, -2);
      index = 0;
      for (const auto& f : schema->fields) {        
        if (f.key == key) {
          type = f.type;          
          break;
        }
        ++index;
      }
    }

    entity_buf[index] = lua_idx_to_var(L, lua_gettop(L));

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

struct GuiLuaState {
  lua_State* L;
  qbComponent component;
};

std::mutex component_tables_mu;
std::unordered_map<qbComponent, qbLuaReference> component_tables;

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
  size_t name_len = std::min(strlen(name) + 1, (size_t)QB_MAX_NAME_LENGTH);
  schema->name = std::string(name).substr(0, name_len);

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

    qbSchemaFieldType_ type = parse_type_string(value);
    size_t size = type.tag == QB_TAG_BYTES ? type.subtag.buffer_size : sizeof(qbVar);

    schema->fields.push_back(qbSchemaField_{ key, type, struct_size, size});
    struct_size += size;

    lua_pop(L, 2);
    return false;
  });
  schema->size = struct_size;
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatasize(attr, sizeof(qbStructInternals_) + struct_size);
    qb_componentattr_setschema(attr, schema);
    qb_component_create(&schema->component, name, attr);
    qb_componentattr_destroy(&attr);
  }

  // Return a new table: { id = new component } with qb.Component as the
  // metatable.
  push_new_table(L, "Component");
  int newtable = lua_gettop(L);

  // Set the return's component.
  lua_pushinteger(L, schema->component);
  lua_setfield(L, newtable, "id");

  {
    std::lock_guard<decltype(component_tables_mu)> l(component_tables_mu);
    lua_pushvalue(L, newtable);
    component_tables[schema->component] = { L, luaL_ref(L, LUA_REGISTRYINDEX) };
  }

  struct GuiLuaState {
    lua_State* L;
    qbComponent component;
  };

  qb_instance_oncreate(schema->component, [](qbInstance instance, qbVar state) {
    lua_State* L = ((GuiLuaState*)state.p)->L;
    qbComponent component = ((GuiLuaState*)state.p)->component;
    {
      qbLuaReference& lua_ref = component_tables[component];
      lua_rawgeti(L, LUA_REGISTRYINDEX, lua_ref.ref);
    }
    int component_table = lua_gettop(L);

    if (lua_getfield(L, component_table, "oncreate") == LUA_TNIL) {
      lua_pop(L, 2);
      return;
    }

    lua_pushvalue(L, component_table);
    lua_newtable(L);
    int entity_table = lua_gettop(L);
    lua_pushinteger(L, qb_instance_entity(instance));
    lua_setfield(L, entity_table, "id");

    lua_getglobal(L, "qb");
      lua_getfield(L, -1, "Entity");
      lua_setmetatable(L, entity_table);
    lua_pop(L, 1);

    qbStruct_* buf;
    qb_instance_const(instance, &buf);
    *(qbStruct_**)lua_newuserdata(L, sizeof(qbStruct_*)) = buf;

    int userdata = lua_gettop(L);

    lua_getglobal(L, "qb");
      lua_getfield(L, -1, "Instance");
      lua_setmetatable(L, userdata);
    lua_pop(L, 1);

    lua_call(L, 3, 0);

    lua_pop(L, 1);
  }, qbPtr(new GuiLuaState{ L, schema->component }));

  qb_instance_ondestroy(schema->component, [](qbInstance instance, qbVar state) {
    lua_State* L = ((GuiLuaState*)state.p)->L;
    qbComponent component = ((GuiLuaState*)state.p)->component;
    {
      qbLuaReference& lua_ref = component_tables[component];
      lua_rawgeti(L, LUA_REGISTRYINDEX, lua_ref.ref);
    }
    int component_table = lua_gettop(L);

    if (lua_getfield(L, component_table, "ondestroy") == LUA_TNIL) {
      lua_pop(L, 2);
      return;
    }

    lua_pushvalue(L, component_table);
    lua_newtable(L);
    int entity_table = lua_gettop(L);
    lua_pushinteger(L, qb_instance_entity(instance));
    lua_setfield(L, entity_table, "id");

    lua_getglobal(L, "qb");
    lua_getfield(L, -1, "Entity");
    lua_setmetatable(L, entity_table);
    lua_pop(L, 1);

    qbStruct_* buf;
    qb_instance_const(instance, &buf);
    *(qbStruct_**)lua_newuserdata(L, sizeof(qbStruct_*)) = buf;

    int userdata = lua_gettop(L);

    lua_getglobal(L, "qb");
    lua_getfield(L, -1, "Instance");
    lua_setmetatable(L, userdata);
    lua_pop(L, 1);

    lua_call(L, 3, 0);

    lua_pop(L, 1);
  }, qbPtr(new GuiLuaState{ L, schema->component }));

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

        qbVar* vars;
        qb_instance_const(insts[i], &vars);
        lua_rawgeti(f->l, LUA_REGISTRYINDEX, f->instance_tables[i]);
        for (size_t i = 0; i < schema->fields.size(); ++i) {
          const auto& field = schema->fields[i];

          switch (field.type.tag) {
            case QB_TAG_LUA_BOOLEAN:
              lua_pushboolean(f->l, vars[i].i);
              break;

            case QB_TAG_DOUBLE:
              lua_pushnumber(f->l, vars[i].d);
              break;

            case QB_TAG_INT:
            case QB_TAG_UINT:
              lua_pushinteger(f->l, vars[i].i);
              break;

            case QB_TAG_CSTRING:
            case QB_TAG_STRING:
              lua_pushstring(f->l, vars[i].s);
              break;

            case QB_TAG_BYTES:
              lua_pushlstring(f->l, vars[i].s, vars[i].size);
              break;

            case QB_TAG_MAP:
            {
              qbVar* map = (qbVar*)lua_newuserdata(f->l, sizeof(qbVar));
              *map = vars[i];
              int userdata = lua_gettop(f->l);

              lua_getglobal(f->l, "qb");
                lua_getfield(f->l, -1, "Map");
                lua_setmetatable(f->l, userdata);
              lua_pop(f->l, 1);
              break;
            }

            case QB_TAG_ARRAY:
            {
              qbVar* array = (qbVar*)lua_newuserdata(f->l, sizeof(qbVar));
              *array = vars[i];

              int userdata = lua_gettop(f->l);

              lua_getglobal(f->l, "qb");
                lua_getfield(f->l, -1, "Array");
                lua_setmetatable(f->l, userdata);
              lua_pop(f->l, 1);
              break;
            }
          }

          lua_setfield(f->l, -2, field.key.c_str());
        }
      }

      lua_call(f->l, (int)f->components.size() + 1, 0);

      for (size_t i = 0; i < f->components.size(); ++i) {
        qbSchema schema = qb_component_schema(f->components[i]);

        qbVar* vars;
        qb_instance_const(insts[i], &vars);

        lua_rawgeti(f->l, LUA_REGISTRYINDEX, f->instance_tables[i]);
        for (size_t j = 0; j < schema->fields.size(); ++j) {

          const auto& field = schema->fields[j];
          lua_getfield(f->l, -1, field.key.c_str());

          switch (field.type.tag) {
            case QB_TAG_LUA_BOOLEAN:
              vars[j] = qbInt(lua_toboolean(f->l, -1));
              break;

            case QB_TAG_DOUBLE:
              vars[j] = qbDouble(lua_tonumber(f->l, -1));
              break;

            case QB_TAG_INT:
            case QB_TAG_UINT:
              vars[j] = qbInt(lua_tointeger(f->l, -1));
              break;

            case QB_TAG_CSTRING:
            case QB_TAG_STRING:
            {
              const char* lua_str = lua_tostring(f->l, -1);
              vars[j] = qbString(lua_str);
              break;
            }

            case QB_TAG_BYTES:
            {
              size_t len;
              const char* str = lua_tolstring(f->l, -1, &len);
              len = std::min(len, field.size);
              vars[j] = qbBytes(str, len);
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

  qbRef r = qb_struct_at(v, key);
  push_var_to_lua(L, *r);
  
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
  qbRef ref = qb_struct_at(v, key);

  if (val.tag != ref->tag) {
    return 0;
  }

  switch (ref->tag) {
    case QB_TAG_LUA_BOOLEAN:
    case QB_TAG_INT:
    case QB_TAG_UINT:
      ref->i = val.i;
      break;

    case QB_TAG_DOUBLE:
      ref->d = val.d;
      break;

    case QB_TAG_STRING:
      free(ref->s);
      ref->s = qbString(val.s).s;
      break;

    case QB_TAG_BYTES:
      memcpy(ref->s, val.s, std::min(ref->size, val.size));
      break;

    case QB_TAG_PTR:
      ref->p = val.p;
      break;
  }

  return 0;
}

qbVar* lua_mapat(lua_State* L, qbVar map, int idx) {
  qbVar* found = nullptr;

  qbTag key_type = qb_map_keytype(map);
  qbVar key = qbNil;
  if (key_type == QB_TAG_ANY) {
    key = lua_idx_to_var(L, idx);
  }

  switch (key_type) {
    case QB_TAG_ANY:
    {
      found = qb_map_at(map, key);
      break;
    }

    case QB_TAG_PTR:
    {
      void* key = lua_touserdata(L, idx);
      found = qb_map_at(map, qbPtr(key));
      break;
    }

    case QB_TAG_INT:
    case QB_TAG_UINT:
    {
      int key = lua_tointeger(L, idx);
      found = qb_map_at(map, qbInt(key));
      break;
    }

    case QB_TAG_DOUBLE:
    {
      double key = lua_tonumber(L, idx);
      found = qb_map_at(map, qbDouble(key));
      break;
    }

    case QB_TAG_STRING:
    {
      const char* key = lua_tostring(L, idx);
      found = qb_map_at(map, qbString(key));
      break;
    }

    case QB_TAG_CSTRING:
    {
      char* key = (char*)lua_touserdata(L, idx);
      found = qb_map_at(map, qbCString(key));
      break;
    }
  }

  return found;
}

int map_get(lua_State* L) {
  if (!lua_isuserdata(L, 1)) {
    lua_pushnil(L);
    return 1;
  }

  qbVar* var = (qbVar*)lua_touserdata(L, 1);
  qbVar* found = lua_mapat(L, *var, 2);

  if (found) {
    push_var_to_lua(L, *found);
  } else {
    push_var_to_lua(L, qbNil);
  }
  
  return 1;
}

int map_set(lua_State* L) {
  if (!lua_isuserdata(L, 1)) {
    return 0;
  }

  qbVar* var = (qbVar*)lua_touserdata(L, 1);  
  qbVar val = lua_idx_to_var(L, 3);
  
  if (val.tag != qb_map_valtype(*var) && qb_map_valtype(*var) != QB_TAG_ANY) {
    return 0;
  }

  *lua_mapat(L, *var, 2) = val;

  return 0;
}

int array_get(lua_State* L) {
  if (!lua_isuserdata(L, 1)) {
    lua_pushnil(L);
    return 1;
  }

  qbVar* v = (qbVar*)lua_touserdata(L, 1);
  int key = lua_tointeger(L, 2) - 1;

  if (key >= qb_array_count(*v)) {
    qb_array_resize(*v, key + 1);
    push_var_to_lua(L, qbNil);
    return 1;
  }

  qbRef r = qb_array_at(*v, key);
  push_var_to_lua(L, *r);

  return 1;
}

int array_set(lua_State* L) {
  if (!lua_isuserdata(L, 1)) {
    return 0;
  }

  qbVar* v = (qbVar*)lua_touserdata(L, 1);
  qbVar val = lua_idx_to_var(L, 3);

  if (val.tag != qb_array_type(*v) && qb_array_type(*v) != QB_TAG_ANY) {
    return 0;
  }

  int key = lua_tointeger(L, 2) - 1;

  if (key >= qb_array_count(*v)) {
    qb_array_resize(*v, key + 1);
  }

  *qb_array_at(*v, key) = val;

  return 0;
}