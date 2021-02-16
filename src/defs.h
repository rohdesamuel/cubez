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

#ifndef DEFS__H
#define DEFS__H

#include "cubez/cubez.h"
#include "component.h"
#include "sparse_map.h"
#include "coro.h"

#include <vector>
#include <functional>
#include <mutex>
#include <unordered_map>

typedef void(*qbInstanceOnCreate)(qbInstance instance, qbVar state);
typedef void(*qbInstanceOnDestroy)(qbInstance instance, qbVar state);

struct qbScene_ {
  const char* name;
  class GameState* state;
  std::vector<std::function<void(qbScene scene, size_t count, const char* keys[], void* values[])>> ondestroy;
  std::vector<std::function<void(qbScene scene, size_t count, const char* keys[], void* values[])>> onactivate;
  std::vector<std::function<void(qbScene scene, size_t count, const char* keys[], void* values[])>> ondeactivate;
  
  std::vector<const char*> keys;
  std::vector<void*> values;
};

struct qbCoro_ {
  Coro main;

  bool is_async;
  std::shared_mutex ret_mu;
  qbVar ret;
  qbVar arg;
};

struct qbInstanceOnCreateEvent_ {
  qbId entity;
  Component* component;
  class GameState* state;
};

struct qbInstanceOnDestroyEvent_ {
  qbId entity;
  Component* component;
  class GameState* state;
};

struct qbComponentAttr_ {
  const char* name;
  qbSchema schema;
  size_t data_size;
  bool is_shared;
  qbComponentType type;

  void(*onpack)(qbComponent component, qbEntity entity, const void* read, qbBuffer* buf, ptrdiff_t* pos);
  void(*onunpack)(qbComponent component, qbEntity entity, void* write, const qbBuffer* read, ptrdiff_t* pos);
};

struct qbBarrier_ {
  void* impl;
};

struct qbTicket_ {
  void* impl;
  std::function<void(void)> lock;
  std::function<void(void)> unlock;
};

struct qbSystemAttr_ {
  qbId program;
  
  qbTransformFn transform;
  qbCallbackFn callback;
  qbConditionFn condition;

  qbTrigger trigger;
  int16_t priority;

  void* state;
  qbComponentJoin join;

  std::vector<qbComponent> constants;
  std::vector<qbComponent> mutables;
  std::vector<qbComponent> components;
  std::vector<qbTicket_*> tickets;
};

enum qbIndexedBy {
  QB_INDEXEDBY_KEY,
  QB_INDEXEDBY_OFFSET,
  QB_INDEXEDBY_HANDLE,
};

struct qbExecutionPolicy_ {
  // OPTIONAL
  // Priority of execution
  int16_t priority;

  // REQUIRED
  // Trigger::LOOP will cause execution in mane game loop. Use Trigger::EVENT
  // to only fire during an event
  qbTrigger trigger;
};

struct qbSystem_ {
  qbId id;
  qbId program;
  void* user_state;
  qbExecutionPolicy_ policy;
};

struct qbComponentInstance_ {
  qbComponent component;
  void* data;
};

struct qbEntityAttr_ {
  std::vector<qbComponentInstance_> component_list;
};

struct qbInstance_ {
  qbInstance_(bool is_mutable = false,
              bool has_schema = false) :
    is_mutable(is_mutable),
    has_schema(has_schema) {};

  qbSystem system;
  Component* component;
  qbEntity entity;
  void* data;
  class GameState* state;

  const bool is_mutable;
  const bool has_schema;
};

struct qbEventAttr_ {
  qbId program;
  size_t message_size;
};

struct qbEvent_ {
  qbId id;
  qbId program;
  void* event;
};

struct qbSchemaFieldType_ {
  qbTag tag;

  union {
    struct {
      qbTag k;
      qbTag v;
    } map;

    qbTag array;
    qbSchema schema;
    size_t buffer_size;
  } subtag;
};

struct qbSchemaField_ {
  std::string key;

  qbSchemaFieldType_ type;

  size_t offset;
  size_t size;
};

struct qbSchema_ {
  std::string name;
  std::vector<qbSchemaField_> fields;
  size_t size;
  qbComponent component;
};

struct qbStructInternals_ {
  qbSchema_* schema;
  qbEntity entity;
};

struct qbStruct_ {
  qbStructInternals_ internals;
  char data;
};

struct qbArray_ {
  qbTag type;
  std::vector<qbVar> array;
};

struct qbStr_ {
  size_t len;
  utf8_t bytes[];
};

struct ComparableVar {
  qbVar v;

  bool operator==(const ComparableVar& other) const {
    if (v.tag != other.v.tag) {
      return false;
    }

    if (v.tag == QB_TAG_STRING || v.tag == QB_TAG_CSTRING) {
      return strcmp(v.s, other.v.s) == 0;
    } else if (v.tag == QB_TAG_BYTES) {
      if (v.size != other.v.size) {
        return false;
      }
      return memcmp(v.p, other.v.p, v.size) == 0;
    }
    return memcmp(v.bytes, other.v.bytes, sizeof(v.bytes)) == 0;
  }
};

namespace std
{
template <>
struct hash<ComparableVar> {
  std::size_t operator()(const ComparableVar& k) const {
    switch (k.v.tag) {
      case QB_TAG_INT:
        return hash<int64_t>()(k.v.i);
      case QB_TAG_BYTES:
      case QB_TAG_PTR:
      case QB_TAG_UINT:
        return hash<uint64_t>()(k.v.u);
      case QB_TAG_DOUBLE:
        return hash<double>()(k.v.d);
      
      case QB_TAG_CSTRING:
        return hash<char*>()(k.v.s);

      case QB_TAG_STRING:
      {
        utf8_t* str = k.v.s;
        unsigned long hash = 5381;
        int c;

        while (c = *str++)
          hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

        return hash;
      }
    }
    return 0;
  }
};
}

struct qbMap_ {
  qbTag k_type;
  qbTag v_type;
  std::unordered_map<ComparableVar, qbVar> map;
};

typedef struct qbSchemaAttrField_ {
  std::string field;
  qbSchemaFieldType_ type;
} qbSchemaAttrField_;

typedef struct qbSchemaAttr_ {
  std::vector<qbSchemaAttrField_> fields;
} qbSchemaAttr_;

#endif
