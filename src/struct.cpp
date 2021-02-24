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

#include <cubez/struct.h>
#include "defs.h"
#include "block_vector.h"

size_t tag_to_size(qbSchemaFieldType_ type) {
  switch (type.tag) {
    case QB_TAG_PTR:
      return sizeof(void*);
    case QB_TAG_INT:
    case QB_TAG_UINT:
      return sizeof(int64_t);
    case QB_TAG_DOUBLE:
      return sizeof(double);
    case QB_TAG_STRING:
      return sizeof(qbStr_*);
    case QB_TAG_ARRAY:
      return sizeof(qbArray_*);
    case QB_TAG_MAP:
      return sizeof(qbMap_*);
    case QB_TAG_BYTES:
      return type.subtag.buffer_size;
    case QB_TAG_STRUCT:
      assert(false && "unimplemented");
  }
  assert(false && "unsupported tag");
}

qbResult qb_schemaattr_create(qbSchemaAttr* attr) {
  *attr = new qbSchemaAttr_{};
  return QB_OK;
}

qbResult qb_schemaattr_addscalar(qbSchemaAttr attr, const char* name, qbTag tag) {
  qbSchemaAttrField_ f;
  f.field = name;
  f.type.tag = tag;

  attr->fields.push_back(f);
  return QB_OK;
}

qbResult qb_schemaattr_addmap(qbSchemaAttr attr, const char* name, qbTag k, qbTag v) {
  qbSchemaAttrField_ f;
  f.field = name;
  f.type.tag = QB_TAG_MAP;
  f.type.subtag.map.k = k;
  f.type.subtag.map.v = v;

  attr->fields.push_back(f);
  return QB_OK;
}

qbResult qb_schemaattr_addarray(qbSchemaAttr attr, const char* name, qbTag v) {
  qbSchemaAttrField_ f;
  f.field = name;
  f.type.tag = QB_TAG_ARRAY;
  f.type.subtag.array = v;

  attr->fields.push_back(f);
  return QB_OK;
}

qbResult qb_schemaattr_addstruct(qbSchemaAttr attr, const char* name, qbSchema schema) {
  qbSchemaAttrField_ f;
  f.field = name;
  f.type.tag = QB_TAG_STRUCT;
  f.type.subtag.schema = schema;

  attr->fields.push_back(f);
  return QB_OK;
}

qbResult qb_schemaattr_addbytes(qbSchemaAttr attr, const char* name, size_t size) {
  qbSchemaAttrField_ f;
  f.field = name;
  f.type.tag = QB_TAG_BYTES;
  f.type.subtag.buffer_size = size;

  attr->fields.push_back(f);
  return QB_OK;
}

qbResult qb_schemaattr_setallocator(qbSchemaAttr attr, qbMemoryAllocator allocator) {
  attr->allocator = allocator;
  return QB_OK;
}

qbResult qb_schemaattr_destroy(qbSchemaAttr* attr) {
  delete *attr;
  *attr = nullptr;
  return QB_OK;
}

qbResult qb_schema_create(qbSchema* schema_ref, const char* name, qbSchemaAttr attr) {
  qbSchema schema = *schema_ref = new qbSchema_;
  for (const auto& attr_f : attr->fields) {
    qbSchemaField_ f;
    f.key = attr_f.field;
    f.type = attr_f.type;
    f.offset = schema->fields.size();

    if (f.type.tag == QB_TAG_BYTES) {
      f.size = f.type.subtag.buffer_size;
    } else {
      f.size = sizeof(qbVar);
    }

    schema->fields.push_back(f);
  }
  schema->name = name;
  schema->size = schema->fields.size() * sizeof(qbVar);
  schema->allocator = attr->allocator ? attr->allocator : qb_memallocator_pool();

  qbComponentAttr cattr;
  qb_componentattr_create(&cattr);
  qb_componentattr_setdatasize(cattr, sizeof(qbStructInternals_) + schema->size);
  qb_componentattr_setschema(cattr, schema);
  qb_component_create(&schema->component, name, cattr);
  qb_componentattr_destroy(&cattr);
  return QB_OK;
}

qbVar qb_struct_create(qbSchema schema, qbVar* fields) {
  qbStruct_* s = (qbStruct_*)qb_alloc(schema->allocator, sizeof(qbStructInternals_) + schema->size);

  s->internals.schema = schema;
  if (fields) {
    memcpy(&s->data, fields, schema->size);
  } else {
    memset(&s->data, 0, schema->size);
  }

  return qbStruct(schema, s);
}

void qb_struct_destroy(qbVar* v) {
  qbStruct_* s = (qbStruct_*)v->p;
  uint8_t i = 0;
  for (auto i = 0; i < s->internals.schema->fields.size(); ++i) {
    qb_var_destroy(qb_struct_ati(*v, i));
    ++i;
  } 
  qbMemoryAllocator allocator = s->internals.schema->allocator;
  qb_dealloc(allocator, s);
  *v = qbNil;
}

qbVar* qb_struct_at(qbVar v, const char* key) {
  qbStruct_* s = (qbStruct_*)v.p;
  for (auto i = 0; i < s->internals.schema->fields.size(); ++i) {
    if (s->internals.schema->fields[i].key == key) {
      return &((qbVar*)(&s->data))[i];
    }
  }

  return nullptr;
}

qbVar* qb_struct_ati(qbVar v, uint8_t i) {
  qbStruct_* s = (qbStruct_*)v.p;
  if (i >= s->internals.schema->fields.size()) {
    return nullptr;
  }

  return &((qbVar*)(&s->data))[i];
}

qbVar* qb_struct_fields(qbVar v) {
  qbStruct_* s = (qbStruct_*)v.p;
  return (qbVar*)(&s->data);
}

const char* qb_struct_keyat(qbVar v, uint8_t i) {
  qbStruct_* s = (qbStruct_*)v.p;
  if (i >= s->internals.schema->fields.size()) {
    return nullptr;
  }

  return s->internals.schema->fields[i].key.data();
}

qbEntity qb_struct_entity(qbVar v) {
  if (v.tag != QB_TAG_STRUCT) {
    return qbInvalidEntity;
  }

  return ((qbStruct_*)v.p)->internals.entity;
}

size_t qb_schema_unpack(qbSchema* schema, const qbBuffer_* buf, ptrdiff_t* pos) {
  ptrdiff_t saved_pos = *pos;
  size_t total = 0;
  size_t bytes_read = 0;

  qbTag tag;
  bytes_read = qb_buffer_read(buf, pos, sizeof(tag), &tag);
  total += bytes_read;
  if (bytes_read != sizeof(tag) || tag != QB_TAG_STRUCT) {
    goto incomplete_read;
  }

  size_t size;
  bytes_read = qb_buffer_readll(buf, pos, (int64_t*)&size);
  total += bytes_read;
  if (bytes_read == 0) {
    goto incomplete_read;
  }

  utf8_t* name;
  size_t len;

  bytes_read = qb_buffer_readstr(buf, pos, &len, &name);
  total += bytes_read;
  if (bytes_read == 0) {
    goto incomplete_read;
  }

  *schema = qb_schema_find(name);
  if (!*schema) {
    goto incomplete_read;
  }

  return total;

incomplete_read:
  *pos = saved_pos;
  return 0;
}

qbResult qb_struct_find(qbVar* v, qbSchema schema, qbEntity entity) {
  qbComponent c = qb_schema_component(schema);

  void* data;
  qbResult res = qb_instance_find(c, entity, &data);
  if (res != QB_OK) {
    return  res;
  }

  *v = qbStruct(schema, data);
  return QB_OK;
}