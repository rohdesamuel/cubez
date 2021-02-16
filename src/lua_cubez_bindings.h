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

#ifndef CUBEZ_LUA_CUBEZ_BINDINGS__H
#define CUBEZ_LUA_CUBEZ_BINDINGS__H

struct lua_State;

/*
*  Args:
*    string: component name
*    list[table[string: field name, string: type name]]: schema
*  Returns:
*    qb.Component: created component
*/
int component_create(lua_State* L);

/*
*  Args:
*    string: component name
*  Returns:
*    int: found component id
*/
int component_find(lua_State* L);

/*
*  Args:
*    int: component id
*  Returns:
*    int: count of entities with given component
*/
int component_count(lua_State* L);

int component_oncreate(lua_State* L);
int component_ondestroy(lua_State* L);

/*
*  Args:
*    list[table[int: component id, table: [string: field name, any: field value]]:
*      the list of created component instances.
*  Returns:
*    int: the id of the created entity
*/
int entity_create(lua_State* L);

/*
*  Args:
*    list[table[int: component id, table: [string: field name, any: field value]]
*  Returns:
*    int: the id of the created entity
*/
int entity_destroy(lua_State* L);

/*
*  Args:
*    int: entity id
*    table[int: component id, table: [string: field name, any: field value]
*/
int entity_addcomponent(lua_State* L);

/*
*  Args:
*    int: entity id
*    int: component id
*/
int entity_removecomponent(lua_State* L);

/*
*  Args:
*    list[table[int: component id, table: [string: field name, any: field value]]
*  Returns:
*    boolean
*/
int entity_hascomponent(lua_State* L);

/*
*  Args:
*    int: entity id
*    int: component id
*  Returns:
*    boolean
*/
int entity_getcomponent(lua_State* L);

int instance_get(lua_State* L);
int instance_set(lua_State* L);

/*
*  Args:
*    list[qb.Component]: list of components to read from
*    function(int: entity, list[any]: components) -> nil: function to transform the components
*  Returns:
*    int: the id of the created system
*/
int system_create(lua_State* L);

int map_get(lua_State* L);
int map_set(lua_State* L);
int array_get(lua_State* L);
int array_set(lua_State* L);

#endif  // CUBEZ_LUA_CUBEZ_BINDINGS__H