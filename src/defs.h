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
  qbValueByKey key;
};

struct qbIterator {
  // REQUIRED
  // Function that returns the beginning of the data to iterate over
  qbData data;

  // REQUIRED
  // Size of step when iterating over data
  size_t stride;

  // OPTIONAL
  // Offset into data to start iterating from. Default 0
  uint32_t offset;
};

struct qbCollection_ {
  const qbId id;
  const char* name;
  const void* self;

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
  qbUpdate update;
  qbInsert insert;
  qbCount count;
};

// All components are keyed on an entity id.
struct qbComponent_ {
  const qbId id;
  size_t data_size;
  qbCollection impl;
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
  qbExecutionPolicy_ policy;
};

struct qbComponentInstance_ {
  qbComponent_* component;
  void* data;
};

struct qbEntityAttr_ {
  std::vector<qbComponentInstance_> component_list;
};

struct qbEntity_ {
  const qbId id;
  const char* name;
  const void* impl;
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

struct qbFrame {
  void* message;
};

#endif
