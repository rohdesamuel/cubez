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

#ifndef CUBEZ_VAR__H
#define CUBEZ_VAR__H

#include <cubez/common.h>

typedef enum {
  QB_TAG_CUSTOM = -1,
  QB_TAG_NIL = 0,
  QB_TAG_FUTURE,
  QB_TAG_ANY,

  // Scalar types.
  QB_TAG_PTR,
  QB_TAG_INT,
  QB_TAG_UINT,
  QB_TAG_DOUBLE,
  QB_TAG_STRING,
  QB_TAG_CSTRING,

  // Container types.
  QB_TAG_STRUCT,
  QB_TAG_ARRAY,
  QB_TAG_MAP,

  // Buffer type.
  QB_TAG_BYTES,
} qbTag;

typedef struct {
  qbTag tag;
  size_t size;

  union {
    void* p;
    utf8_t* s;
    int64_t i;
    uint64_t u;
    double d;

    char bytes[8];
  };
} qbVar;

typedef qbVar* qbRef;
typedef struct qbSchema_* qbSchema;
typedef struct qbSchemaField_* qbSchemaField;

// Convenience functions for creating a qbVar. 
QB_API qbVar       qbPtr(void* p);
QB_API qbVar       qbInt(int64_t i);
QB_API qbVar       qbUint(uint64_t u);
QB_API qbVar       qbDouble(double d);
QB_API qbVar       qbStruct(qbSchema schema, void* buf);
QB_API qbVar       qbArray(qbTag v_type);
QB_API qbVar       qbMap(qbTag k_type, qbTag v_type);
QB_API qbVar       qbString(const utf8_t* s);
QB_API qbVar       qbCString(utf8_t* s);
QB_API qbVar       qbBytes(const char* bytes, size_t size);

QB_API qbRef       qb_ref_at(qbVar v, qbVar key);
QB_API qbVar       qb_var_at(qbVar v, qbVar key);

QB_API qbVar*      qb_array_at(qbVar array, int32_t i);
QB_API qbVar*      qb_array_append(qbVar array, qbVar v);
QB_API void        qb_array_insert(qbVar array, qbVar v, int32_t i);
QB_API void        qb_array_erase(qbVar array, int32_t i);
QB_API void        qb_array_swap(qbVar array, int32_t i, int32_t j);
QB_API void        qb_array_resize(qbVar array, size_t size);
QB_API size_t      qb_array_count(qbVar array);
QB_API qbTag       qb_array_type(qbVar array);
QB_API qbVar*      qb_array_raw(qbVar array);
QB_API qbBool        qb_array_iterate(qbVar array, qbBool(*it)(qbVar* k, qbVar state), qbVar state);

QB_API qbVar*      qb_map_at(qbVar map, qbVar k);
QB_API qbVar*      qb_map_insert(qbVar map, qbVar k, qbVar v);
QB_API void        qb_map_erase(qbVar map, qbVar k);
QB_API void        qb_map_swap(qbVar map, qbVar a, qbVar b);
QB_API size_t      qb_map_count(qbVar map);
QB_API qbBool        qb_map_has(qbVar map, qbVar k);
QB_API qbTag       qb_map_keytype(qbVar map);
QB_API qbTag       qb_map_valtype(qbVar map);
QB_API qbBool        qb_map_iterate(qbVar map, qbBool(*it)(qbVar k, qbVar* v, qbVar state), qbVar state);

QB_API size_t      qb_var_pack(qbVar v, struct qbBuffer_* buf, ptrdiff_t* pos);
QB_API size_t      qb_var_unpack(qbVar* v, const struct qbBuffer_* buf, ptrdiff_t* pos);
QB_API void        qb_var_copy(const qbVar* from, qbVar* to);
QB_API void        qb_var_move(qbVar* from, qbVar* to);
QB_API void        qb_var_destroy(qbVar* v);
QB_API qbVar       qb_var_tostring(qbVar v);

// Represents a non-value.
QB_API extern const qbVar qbNil;

// Represents a qbVar that is not yet set. Used to represent a value that will
// be set in the future.
QB_API extern const qbVar qbFuture;

#endif  // CUBEZ_VAR__H