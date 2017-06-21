/**
* Author: Samuel Rohde (rohde.samuel@gmail.com)
*
* This file is subject to the terms and conditions defined in
* file 'LICENSE.txt', which is part of this source code package.
*/

#ifndef CUBEZ__H
#define CUBEZ__H

#include "common.h"
#include "universe.h"

BEGIN_EXTERN_C

// Holds the game engine state.
struct qbUniverse {
  void* self;
};

struct qbElement {
  uint8_t* data;
  size_t size;
};

enum class qbMutateBy {
  UNKNOWN = 0,
  INSERT,
  UPDATE,
  REMOVE,
  INSERT_OR_UPDATE,
};

enum class qbIndexedBy {
  UNKNOWN = 0,
  OFFSET,
  HANDLE,
  KEY
};

struct qbMutation {
  qbMutateBy mutate_by;
  void* element;
};

struct qbArg {
  void* data;
  size_t size;
};

struct qbArgs {
  qbArg* arg;
  uint8_t count;
};

enum class qbTrigger {
  LOOP = 0,
  EVENT,
};

const int16_t QB_MAX_PRIORITY = 0x7FFF;
const int16_t QB_MIN_PRIORITY = 0x8001;

struct qbExecutionPolicy {
  // OPTIONAL.
  // Priority of execution.
  int16_t priority;

  // REQUIRED.
  // Trigger::LOOP will cause execution in mane game loop. Use Trigger::EVENT
  // to only fire during an event.
  qbTrigger trigger;
};

struct qbProgram {
  const qbId id;
  const char* name;
  const void* self;
};

qbResult qb_init(struct qbUniverse* universe);
qbResult qb_start();
qbResult qb_stop();
qbResult qb_loop();
qbResult qb_run_program(qbId program);

qbId qb_create_program(const char* name);

///////////////////////////////////////////////////////////
////////////////////////  Systems  ////////////////////////
///////////////////////////////////////////////////////////

typedef bool (*qbSelect)(struct qbSystem* system, struct qbFrame*);
typedef void (*qbTransform)(struct qbSystem* system, struct qbFrame*);
typedef void (*qbCallback)(struct qbSystem* system, struct qbFrame*,
                         const struct qbCollections* sources,
                         const struct qbCollections* sinks);

struct qbSystem {
  const qbId id;
  const qbId program;
  const void* self;

  // OPTIONAL.
  // Pointer to user-defined state.
  void* state;

  // OPTIONAL.
  // If defined, will run for each object in collection.
  qbTransform transform;

  // OPTIONAL.
  // If defined, will run after all objects have been processed with
  // 'transform'.
  qbCallback callback;
};

// Allocates and adds a system to the specified program. System is enabled for
// execution by default.
// Thread-safe:
// Idempotent:
struct qbSystem* qb_alloc_system(const char* program, const char* source,
                                  const char* sink);

// Free a system.
// Thread-safe:
// Idempotent:
qbResult qb_free_system(struct qbSystem* system);

// Copies a system and its configuration to a different program.
struct qbSystem* qb_copy_system(struct qbSystem* source_system,
                                const char* destination_program);

qbResult qb_enable_system(struct qbSystem* system, qbExecutionPolicy policy);
qbResult qb_disable_system(struct qbSystem* system);

qbResult qb_add_source(struct qbSystem*, const char* collection);

///////////////////////////////////////////////////////////
//////////////////////  Collections  //////////////////////
///////////////////////////////////////////////////////////

typedef void (*qbMutate)(struct qbCollection*, const struct qbMutation*);
typedef void (*qbCopy)(const uint8_t* key, const uint8_t* value, uint64_t index,
                       struct qbFrame*);
typedef uint64_t (*qbCount)(struct qbCollection*);
typedef uint8_t* (*qbData)(struct qbCollection*);
typedef void* (*qbAccessor)(struct qbCollection*, qbIndexedBy indexed_by,
                            const void* index);

struct qbIterator {
  qbData data;
  uint32_t offset;
  size_t size;
};

struct qbCollection {
  const qbId id;
  const char* name;
  const void* self;

  // REQUIRED.
  void* collection;

  // REQUIRED.
  qbAccessor accessor;

  // REQUIRED.
  qbIterator keys;

  // REQUIRED.
  qbIterator values;
  
  // REQUIRED.
  qbCopy copy;

  // REQUIRED.
  qbMutate mutate;

  // REQUIRED.
  qbCount count;
};

struct qbCollections {
  uint64_t count;
  struct qbCollection** collection;
};

struct qbCollection* qb_alloc_collection(const char* program, const char* name);
struct qbCollection* qb_create_collection_pool(size_t value_size);
struct qbCollection* qb_create_collection_table(size_t key_size,
                                                size_t value_size);

qbResult qb_free_collection(struct qbCollection* collection);
qbResult qb_share_collection(
    const char* source_program, const char* source_collection,
    const char* dest_program, const char* dest_collection);
qbResult qb_copy_collection(
    const char* source_program, const char* source_collection,
    const char* dest_program, const char* dest_collection);


///////////////////////////////////////////////////////////
//////////////////  Events and Messaging  /////////////////
///////////////////////////////////////////////////////////

struct qbMessage {
  struct qbChannel* channel;
  void* data;

  size_t size;
};

struct qbKey {
  void* data;
  size_t size;
};

struct qbKeys {
  uint8_t count;
  struct qbKey* key;
};

struct qbFrame {
  const void* self;
  qbArgs args;

  qbMutation mutation;
  qbMessage message;
};

struct qbChannel {
  const qbId program;
  const qbId event;
  void* self;
};

struct qbSubscription {
  const qbId program;
  const qbId event;
  const qbId system;
};

struct qbEventPolicy {
  size_t size;
};

// Thread-safe.
qbId qb_create_event(const char* program, const char* event, qbEventPolicy policy);

// Flush events from program. If event if null, flush all events from program.
// Engine must not be in the LOOPING run state.
qbResult qb_flush_events(const char* program, const char* event);

struct qbMessage* qb_alloc_message(struct qbChannel* channel);
void qb_send_message(qbMessage* e);

struct qbChannel* qb_open_channel(const char* program, const char* event);
void qb_close_channel(struct qbChannel* channel);

// Subscribe a system to receive messages from an event.
struct qbSubscription* qb_subscribe_to(const char* program, const char* event,
                                       struct qbSystem* system);
// Ubsubscribe a system from receiving events.
void qb_unsubscribe_from(struct qbSubscription* subscription);

END_EXTERN_C

#endif  // #ifndef CUBEZ__H
