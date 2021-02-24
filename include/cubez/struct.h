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

#ifndef CUBEZ_STRUCT__H
#define CUBEZ_STRUCT__H

#include <cubez/cubez.h>
#include <cubez/memory.h>

typedef struct qbSchemaAttr_* qbSchemaAttr;
QB_API qbResult qb_schemaattr_create(qbSchemaAttr* attr);
QB_API qbResult qb_schemaattr_destroy(qbSchemaAttr* attr);
QB_API qbResult qb_schemaattr_addscalar(qbSchemaAttr attr, const char* name, qbTag tag);
QB_API qbResult qb_schemaattr_addmap(qbSchemaAttr attr, const char* name, qbTag k, qbTag v);
QB_API qbResult qb_schemaattr_addarray(qbSchemaAttr attr, const char* name, qbTag k);
QB_API qbResult qb_schemaattr_addbytes(qbSchemaAttr attr, const char* name, size_t size);
QB_API qbResult qb_schemaattr_setallocator(qbSchemaAttr attr, qbMemoryAllocator allocator);
QB_API qbResult qb_schema_create(qbSchema* schema, const char* name, qbSchemaAttr attr);

// Loads the schema from the script directory set in the qbScriptAttr_.
QB_API qbResult qb_schema_load(qbSchema* schema, const char* filename);

// Finds a schema by name.
QB_API qbSchema    qb_schema_find(const char* name);

// Returns the size of the schema in bytes. This can be used to allocate a new
// instance of the schema.
QB_API size_t      qb_schema_size(qbSchema schema);
QB_API qbVar*      qb_struct_at(qbVar v, const char* key);
QB_API qbVar*      qb_struct_ati(qbVar v, uint8_t i);
QB_API qbVar*      qb_struct_fields(qbVar v);
QB_API const char* qb_struct_keyat(qbVar v, uint8_t i);
QB_API uint8_t     qb_struct_numfields(qbVar v);
QB_API qbSchema    qb_struct_getschema(qbVar v);
QB_API qbVar       qb_struct_create(qbSchema schema, qbVar* fields);
QB_API void        qb_struct_destroy(qbVar* v);
QB_API qbResult    qb_struct_find(qbVar* v, qbSchema schema, qbEntity entity);

// Returns the size of a dynamically created struct from a qbSchema in bytes.
// Can be used like:
// size_t size = qb_struct_size(schema);
// void* s = malloc(size);
// qbVar v = qbStruct(schema, s);
// qbRef a = qb_var_at(v, "a");
QB_API size_t      qb_struct_size(qbSchema schema);

// Returns the field definitions of the schema.
QB_API uint8_t      qb_schema_fields(qbSchema schema, qbSchemaField* fields);

QB_API const char* qb_schema_name(qbSchema schema);
QB_API qbComponent qb_schema_component(qbSchema schema);
QB_API uint8_t     qb_schema_numfields(qbSchema schema);

// Returns the name of the field.
QB_API const char* qb_schemafield_name(qbSchemaField field);

// Returns the tag type of the field.
QB_API qbTag       qb_schemafield_tag(qbSchemaField field);

// Returns the offset of the field in bytes.
QB_API size_t      qb_schemafield_offset(qbSchemaField field);

// Returns the size of the field in bytes.
QB_API size_t      qb_schemafield_size(qbSchemaField field);


#endif  // CUBEZ_STRUCT__H