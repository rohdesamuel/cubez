#ifndef DEFS__H
#define DEFS__H

#include "common.h"
#include "cubez.h"

#include <vector>


struct qbEntityId_ {
  qbHandle* handle;
};

struct qbAccessor {
  // REQUIRED
  qbValueByOffset offset;

  // REQUIRED
  qbValueByHandle handle;

  // REQUIRED
  qbValueById id;
};

struct qbIterator {
  // REQUIRED
  // Function that returns the beginning of the data to iterate over
  qbData data;

  // REQUIRED
  // Size of step when iterating over data
  size_t stride;

  size_t size;

  // OPTIONAL
  // Offset into data to start iterating from. Default 0
  uint32_t offset;
};

struct qbCollection_ {
  const qbId id;
  const qbId program_id;

  qbIterator keys;
  qbIterator values;
  qbCount count;

  qbCollectionInterface interface;
};

struct qbCollectionAttr_ {
  void* collection;
  const char* program;

  qbAccessor accessor;
  qbIterator keys;
  qbIterator values;
  qbInsert insert;
  qbCount count;
  qbRemoveByOffset remove_by_offset;
  qbRemoveByHandle remove_by_handle;
  qbRemoveById remove_by_id;
};

typedef void(*qbComponentOnCreate)(qbEntity parent_entity,
                                   qbComponent to_destroy,
                                   void* instance_data);

typedef void(*qbComponentOnDestroy)(qbEntity parent_entity,
                                    qbComponent to_destroy,
                                    void* instance_data);

struct qbComponentOnCreateEvent_ {
  qbEntity entity;
  qbComponent component;
  void* instance_data;
};

struct qbComponentOnDestroyEvent_ {
  qbEntity entity;
  qbComponent component;
  void* instance_data;
};

// All components are keyed on an entity id.
struct qbComponent_ {
  const qbId id;
  size_t data_size;
  qbCollection impl;

  qbEvent on_create;
  qbEvent on_destroy;
};

struct qbComponentAttr_ {
  const char* program;
  const char* name;
  size_t data_size;
  qbCollection impl;
};

struct qbSystemAttr_ {
  const char* program;
  
  qbTransform transform;
  qbCallback callback;

  qbTrigger trigger;
  int16_t priority;

  void* state;
  qbComponentJoin join;

  std::vector<qbComponent_*> sources;
  std::vector<qbComponent_*> sinks;
};

enum qbIndexedBy {
  QB_INDEXEDBY_KEY,
  QB_INDEXEDBY_OFFSET,
  QB_INDEXEDBY_HANDLE,
};

struct qbElement_ {
  qbId id;
  void* read_buffer;
  void* user_buffer;
  size_t size;

  union {
    qbOffset offset;
    qbHandle handle;
  };
  qbIndexedBy indexed_by;
  qbCollectionInterface interface;
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
  const qbId id;
  const qbId program;
  void* user_state;
  qbExecutionPolicy_ policy;
};

struct qbComponentInstance_ {
  qbComponent_* component;
  void* data;
};

struct qbEntityAttr_ {
  std::vector<qbComponentInstance_> component_list;
};

struct qbInstance_ {
  qbComponent component;
  qbHandle instance_handle;
  void* data;
};

struct qbEntity_ {
  const qbId id;
  std::vector<qbInstance_> instances;

  qbEvent destroy_event;
};

struct qbEventAttr_ {
  const char* program;
  size_t message_size;
};

struct qbEvent_ {
  const qbId id;
  const qbId program;
  void* channel;
};

#endif
