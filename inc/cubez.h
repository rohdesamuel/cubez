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

#define CONST_P(type) const type* const 

BEGIN_EXTERN_C

#ifdef __cplusplus
namespace cubez {
#endif

struct Element {
  uint8_t* data;
  size_t size;
};

enum class MutateBy {
  UNKNOWN = 0,
  INSERT,
  UPDATE,
  REMOVE,
  INSERT_OR_UPDATE,
};

enum class IndexedBy {
  UNKNOWN = 0,
  OFFSET,
  HANDLE,
  KEY
};

struct Mutation {
  MutateBy mutate_by;
  void* element;
};

struct Arg {
  void* data;
  size_t size;
};

struct Args {
  Arg* arg;
  uint8_t count;
};

Arg* get_arg(struct Frame* frame, const char* name);
Arg* new_arg(struct Frame* frame, const char* name, size_t size);
Arg* set_arg(struct Frame* frame, const char* name, void* data, size_t size);

typedef bool (*Select)(struct Pipeline* pipeline, struct Frame*);
typedef void (*Transform)(struct Pipeline* pipeline, struct Frame*);
typedef void (*Callback)(struct Pipeline* pipeline, struct Frame*,
                         const struct Collections* sources,
                         const struct Collections* sinks);

typedef void (*Mutate)(struct Collection*, const struct Mutation*);
typedef void (*Copy)(const uint8_t* key, const uint8_t* value, uint64_t index, struct Frame*);
typedef uint64_t (*Count)(struct Collection*);
typedef uint8_t* (*Data)(struct Collection*);
typedef void* (*Accessor)(struct Collection*, IndexedBy indexed_by, const void* index);

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

  Accessor accessor;

  Iterator keys;
  Iterator values;
  
  Copy copy;
  Mutate mutate;
  Count count;
};

struct Collections {
  uint64_t count;
  struct Collection** collection;
};

enum class Trigger {
  LOOP = 0,
  EVENT,
};

const int16_t MAX_PRIORITY = 0x7FFF;
const int16_t MIN_PRIORITY = 0x8001;

struct ExecutionPolicy {
  // OPTIONAL.
  // Priority of execution.
  int16_t priority;

  // REQUIRED.
  // Trigger::LOOP will cause execution in mane game loop. Use Trigger::EVENT
  // to only fire during an event.
  Trigger trigger;
};

struct Pipeline {
  const Id id;
  const Id program;
  const void* self;

  // OPTIONAL.
  // Pointer to user-defined state.
  void* state;

  // OPTIONAL.
  // If defined, will run for each object in collection.
  Transform transform;

  // OPTIONAL.
  // If defined, will run after all objects have been processed with
  // 'transform'.
  Callback callback;
};

struct Program {
  const Id id;
  const char* name;
  const void* self;
};


Status::Code init(struct Universe* universe);
Status::Code start();
Status::Code stop();
Status::Code loop();
Status::Code run_program(Id program);

Id create_program(const char* name);

// Pipelines.
struct Pipeline* add_pipeline(const char* program, const char* source, const char* sink);
struct Pipeline* copy_pipeline(struct Pipeline* pipeline, const char* dest);

Status::Code remove_pipeline(struct Pipeline* pipeline);
Status::Code enable_pipeline(struct Pipeline* pipeline, ExecutionPolicy policy);
Status::Code disable_pipeline(struct Pipeline* pipeline);

Status::Code add_source(struct Pipeline*, const char* collection);
Status::Code add_sink(struct Pipeline*, const char* collection);
Status::Code add_keys(struct Pipeline*, uint8_t count, ...);

// Collections.
Collection* add_collection(const char* program, const char* name);
Status::Code share_collection(/*const char* program*/ const char* source, const char* dest);
Status::Code copy_collection(/*const char* program*/ const char* source, const char* dest);

struct Message {
  struct Channel* channel;
  void* data;

  size_t size;
};

struct Key {
  void* data;
  size_t size;
};

struct Keys {
  uint8_t count;
  struct Key* key;
};

struct Frame {
  const void* self;
  Args args;

  Mutation mutation;
  Message message;
};

struct Channel {
  const Id program;
  const Id event;
  void* self;
};

struct Subscription {
  const Id program;
  const Id event;
  const Id pipeline;
};

struct EventPolicy {
  size_t size;
};

// Thread-safe.
Id create_event(const char* program, const char* event, EventPolicy policy);

// Flush events from program. If event if null, flush all events from program.
// Engine must not be in the LOOPING run state.
Status::Code flush_events(const char* program, const char* event);

Message* new_message(struct Channel* channel);
void send_message(Message* e);

struct Channel* open_channel(const char* program, const char* event);
void close_channel(struct Channel* channel);

struct Subscription* subscribe_to(
    const char* program, const char* event, struct Pipeline* pipeline);
void unsubscribe_from(struct Subscription* subscription);

#ifdef __cplusplus
}  // namespace cubez
#endif

END_EXTERN_C

#endif  // #ifndef RADIANCE__H
