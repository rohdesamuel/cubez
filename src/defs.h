#ifndef DEFS__H
#define DEFS__H

#include "common.h"
#include "component.h"
#include "cubez.h"
#include "sparse_map.h"

#include <vector>
#include <functional>

typedef void(*qbInstanceOnCreate)(qbInstance instance);
typedef void(*qbInstanceOnDestroy)(qbInstance instancea);

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
  size_t data_size;
  bool is_shared;
  qbComponentType type;
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
  
  qbTransform transform;
  qbCallback callback;
  qbCondition condition;

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
  qbInstance_(bool is_mutable = false) : is_mutable(is_mutable) {};

  qbSystem system;
  Component* component;
  qbEntity entity;
  void* data;
  class GameState* state;

  const bool is_mutable;
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

#endif
