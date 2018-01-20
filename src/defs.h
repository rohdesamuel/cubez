#ifndef DEFS__H
#define DEFS__H

#include "common.h"
#include "component.h"
#include "cubez.h"
#include "sparse_map.h"

#include <vector>


struct qbEntityId_ {
  qbHandle* handle;
};

typedef void(*qbInstanceOnCreate)(qbInstance instance);
typedef void(*qbInstanceOnDestroy)(qbInstance instancea);

struct qbInstanceOnCreateEvent_ {
  qbId entity;
  qbComponent component;
};

struct qbInstanceOnDestroyEvent_ {
  qbId entity;
  qbComponent component;
};

// All components are keyed on an entity id.
struct qbComponent_ {
  Component instances;
};

struct qbComponentAttr_ {
  const char* program;
  const char* name;
  size_t data_size;
};

struct qbSystemAttr_ {
  const char* program;
  
  qbTransform transform;
  qbCallback callback;

  qbTrigger trigger;
  int16_t priority;

  void* state;
  qbComponentJoin join;

  std::vector<qbComponent_*> constants;
  std::vector<qbComponent_*> mutables;
  std::vector<qbComponent_*> components;
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
  qbSystem system;
  qbComponent component;
  qbEntity entity;
  void* data;

  bool is_mutable;
};

struct qbEventAttr_ {
  const char* program;
  size_t message_size;
};

struct qbEvent_ {
  qbId id;
  qbId program;
  void* event;
};

#endif
