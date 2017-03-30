/**
* Author: Samuel Rohde (rohde.samuel@gmail.com)
*
* This file is subject to the terms and conditions defined in
* file 'LICENSE.txt', which is part of this source code package.
*/

#ifndef RADIANCE__H
#define RADIANCE__H

#include "common.h"
#include "universe.h"

BEGIN_EXTERN_C

#ifdef __cplusplus
namespace cubez {
#endif

struct Element {
  uint8_t* data;
  size_t size;
};

struct TypedElement {
  const Id collection;
  Element element;
};

struct Tuple {
  uint64_t count;
  TypedElement* element;
};

enum class MutateBy {
  UNKNOWN = 0,
  INSERT,
  UPDATE,
  REMOVE,
  INSERT_OR_UPDATE,
};

struct Mutation {
  MutateBy mutate_by;
  uint8_t* element;
};

typedef bool (*Select)(uint8_t, ...);
typedef void (*Transform)(struct Stack*);
typedef void (*Callback)(struct Pipeline*, struct Collections sources, struct Collections sinks);

typedef void (*Mutate)(struct Collection*, const struct Mutation*);
typedef void (*Copy)(const uint8_t* key, const uint8_t* value, uint64_t index, struct Stack*);
typedef uint64_t (*Count)(struct Collection*);
typedef uint8_t* (*Data)(struct Collection*);

struct Iterator {
  Data data;
  uint32_t offset;
  size_t size;
};

struct Collection {
  const Id id;
  const char* name;
  const void* self;

  void* collection;

  Iterator keys;
  Iterator values;
  
  Copy copy;
  Mutate mutate;
  Count count;
};

struct Collections {
  uint64_t count;
  struct Collection** collections;
};

enum class Trigger {
  UNKNOWN = 0,
  LOOP,
  EVENT,
};

const int16_t MAX_PRIORITY = 0x7FFF;
const int16_t MIN_PRIORITY = 0x8001;

struct GameState {
  uint64_t frame;
  uint64_t timestamp_ns;
  uint64_t prev_timestamp_ns;

  struct Keys* down;
  struct Keys* up;
  struct Keys* change;
};

typedef bool (*Event)(GameState*);

/*
extern Event KeyPressed;
extern Event KeyReleased;
extern Event Timer;
*/
struct EventPolicy {
  Event event;
};

struct ExecutionPolicy {
  int16_t priority;
  Trigger trigger;
};

struct Pipeline {
  const Id id;
  const Id program;
  const void* self;

  Select select;
  Transform transform;
  Callback callback;
};

struct Program {
  const Id id;
  const char* name;
  const void* self;
};

Status::Code init(Universe* universe);
Status::Code start();
Status::Code stop();
Status::Code loop();

Id create_program(const char* name);
Id detach_program(const char* name);
Id join_program(const char* name);

// Pipelines.
struct Pipeline* add_pipeline(const char* program, const char* source, const char* sink);
struct Pipeline* copy_pipeline(struct Pipeline* pipeline, const char* dest);
Status::Code remove_pipeline(struct Pipeline* pipeline);
Status::Code enable_pipeline(struct Pipeline* pipeline, ExecutionPolicy policy);
Status::Code disable_pipeline(struct Pipeline* pipeline);

/*
struct Event {
  void* data;
  size_t size;
};
void send_event(struct Pipeline* pipeline, Event event);
*/
Status::Code add_source(struct Pipeline*, const char* collection);
Status::Code add_sink(struct Pipeline*, const char* collection);

// Collections.
Collection* add_collection(const char* program, const char* name);
Status::Code share_collection(const char* source, const char* dest);
Status::Code copy_collection(const char* source, const char* dest);

#ifdef __cplusplus
}  // namespace cubez
#endif

END_EXTERN_C

#endif  // #ifndef RADIANCE__H
