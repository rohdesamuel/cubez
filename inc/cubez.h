/**
* Author: Samuel Rohde (rohde.samuel@gmail.com)
*
* This file is subject to the terms and conditions defined in
* file 'LICENSE.txt', which is part of this source code package.
*/

#ifndef CUBEZ__H
#define CUBEZ__H

#include "common.h"

BEGIN_EXTERN_C

///////////////////////////////////////////////////////////
//////////////////////  Flow Control  /////////////////////
///////////////////////////////////////////////////////////

// Holds the game engine state
struct qbUniverse {
  void* self;
};

qbResult qb_init(struct qbUniverse* universe);
qbResult qb_start();
qbResult qb_stop();
qbResult qb_loop();

// A program is a structure that holds all encapsulated state. This includes
// collections, systems, events, and subscriptions. Programs can only talk to
// each other through messages. Programs can access each other's collections
// only after it has been shared.
struct qbProgram {
  const qbId id;
  const char* name;
  const void* self;
};

// Arguments:
//   name The program name to refer to
//
// Returns:
//   A unique Id
qbId qb_create_program(const char* name);

// Runs a particular program flushing its events and running all of its systems
//
// Arguments:
//   program The program Id to flush
//
// Returns:
//   QB_OK on success
qbResult qb_run_program(qbId program);

// Detaches a program from the main game loop. This starts an asynchronous
// thread.
//
// Arguments:
//   program The program Id to detach
//
// Returns:
//   QB_OK on success
qbResult qb_detach_program(qbId program);

// Joins a program with the main game loop.
//
// Arguments:
//   program The program Id to join
//
// Returns:
//   QB_OK on success
qbResult qb_join_program(qbId program);

///////////////////////////////////////////////////////////
////////////////////////  Systems  ////////////////////////
///////////////////////////////////////////////////////////

const int16_t QB_MAX_PRIORITY = 0x7FFF;
const int16_t QB_MIN_PRIORITY = 0x8001;

enum class qbTrigger {
  LOOP = 0,
  EVENT,
};

struct qbExecutionPolicy {
  // OPTIONAL
  // Priority of execution
  int16_t priority;

  // REQUIRED
  // Trigger::LOOP will cause execution in mane game loop. Use Trigger::EVENT
  // to only fire during an event
  qbTrigger trigger;
};

typedef bool (*qbSelect)(struct qbSystem* system, struct qbFrame*);
typedef void (*qbTransform)(struct qbSystem* system, struct qbFrame*);
typedef void (*qbCallback)(struct qbSystem* system, struct qbFrame*,
                           const struct qbCollections* sources,
                           const struct qbCollections* sinks);

struct qbArg {
  void* data;
  size_t size;
};

struct qbArgs {
  qbArg* arg;
  uint8_t count;
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

struct qbElement {
  void* index;
  void* value;

  qbIndexedBy indexed_by;
};

struct qbMutation {
  // REQUIRED
  qbMutateBy mutate_by;
  
  // REQUIRED
  // Pointer to a piece of memory to be passed 
  void* const element;
};

struct qbMessage {
  // What channel this message belongs to.
  struct qbChannel* channel;

  void* const data;
  size_t size;
};

struct qbFrame {
  const void* self;
  qbArgs args;

  qbMutation mutation;
  qbMessage message;
};

struct qbSystem {
  const qbId id;
  const qbId program;
  const void* self;

  // OPTIONAL
  // Pointer to user-defined state
  void* state;

  // OPTIONAL
  // If defined, will run for each object in collection
  qbTransform transform;

  // OPTIONAL
  // If defined, will run after all objects have been processed with
  // 'transform'
  qbCallback callback;
};

// Allocate, take ownership, and add a system to the specified program. System
// is enabled for execution by default
//
// Arguments:
//   program The program name to attach this system to
//   source The collection name to read from. If there are multiple sources,
//          this collection's key will be used to query all sources added with
//          qb_add_source.
//   sink The collection name to write to
//
// Returns:
//   The allocated system
struct qbSystem* qb_alloc_system(const char* program, const char* source,
                                  const char* sink);

// Relinquish ownership to engine and free it
//
// Arguments:
//   system The system to free
//
// Returns:
//   QB_OK on success
qbResult qb_free_system(struct qbSystem* system);

// Copies a system and its configuration to a different program
//
// Arguments:
//   source_system The system to copy
//   destination_program The program to copy the system to
//
// Returns:
//   Pointer to newly allocated system
struct qbSystem* qb_copy_system(struct qbSystem* source_system,
                                const char* destination_program);

// Enable a system to run.
//
// Arguments:
//   system The system to enable
//   policy The new policy to set
//
// Returns:
//   QB_OK on success
qbResult qb_enable_system(struct qbSystem* system, qbExecutionPolicy policy);

// Disable a system from running
//
// Arguments:
//   system The system to disable
//
// Returns:
//   QB_OK on success
qbResult qb_disable_system(struct qbSystem* system);

// Adds a collection for a system to read from. The collection must be in the
// same program as the system.
//
// Arguments:
//   system The system to add a collection to
//   collection The collection name to add
//
// Returns:
//   QB_OK on success
qbResult qb_add_source(struct qbSystem* system, const char* collection);

// Adds a collection for a system to write to. The collection must be in the
// same program as the system.
//
// Arguments:
//   system The system to add a collection to
//   collection The collection name to add
//
// Returns:
//   QB_OK on success
qbResult qb_add_sink(struct qbSystem* system, const char* collection);

///////////////////////////////////////////////////////////
//////////////////////  Collections  //////////////////////
///////////////////////////////////////////////////////////

typedef void (*qbMutate)(struct qbCollection*, const struct qbMutation*);
typedef void (*qbCopy)(const uint8_t* key, const uint8_t* value, uint64_t index,
                       struct qbFrame*);
typedef uint64_t (*qbCount)(struct qbCollection*);
typedef uint8_t* (*qbData)(struct qbCollection*);
typedef void* (*qbValueByOffset)(struct qbCollection*, uint64_t offset);
typedef void* (*qbValueByHandle)(struct qbCollection*, qbHandle handle);
typedef void* (*qbValueByKey)(struct qbCollection*, void* key);

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

struct qbCollection {
  const qbId id;
  const char* name;
  const void* self;

  // REQUIRED
  void* collection;

  // REQUIRED
  qbAccessor accessor;

  // REQUIRED
  qbIterator keys;

  // REQUIRED
  qbIterator values;
  
  // REQUIRED
  qbCopy copy;

  // REQUIRED
  qbMutate mutate;

  // REQUIRED
  qbCount count;
};

struct qbCollections {
  uint64_t count;
  struct qbCollection** collection;
};

struct qbCollection* qb_alloc_collection(const char* program, const char* name);

qbResult qb_free_collection(struct qbCollection* collection);
qbResult qb_share_collection(
    const char* source_program, const char* source_collection,
    const char* dest_program, const char* dest_collection);
qbResult qb_copy_collection(
    const char* source_program, const char* source_collection,
    const char* dest_program, const char* dest_collection);

///////////////////////////////////////////////////////////
/////////////////////  Type Management  ///////////////////
///////////////////////////////////////////////////////////

struct qbEntity {
  const qbId id;
  const char* name;
  const void* self;
};

struct qbInstance {
  const qbId id;
  const qbId collection;
  void* const data;
};

// 
qbId qb_define_entity(const char* program, const char* name,
                      struct qbEntity* entity);

qbResult qb_add_collection(struct qbEntity* entity,
                           struct qbCollection* collection);

qbResult qb_create_entity(struct qbEntity* entity, uint32_t count,
                          qbInstance created[]);




///////////////////////////////////////////////////////////
//////////////////  Events and Messaging  /////////////////
///////////////////////////////////////////////////////////

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

// Thread-safe
qbId qb_create_event(const char* program, const char* event,
                     qbEventPolicy policy);

// Flush events from program. If event if null, flush all events from program.
// Engine must not be in the LOOPING run state.
qbResult qb_flush_events(const char* program, const char* event);

// Thread-safe.
struct qbMessage* qb_alloc_message(struct qbChannel* channel);

// Thread-safe.
void qb_send_message(qbMessage* e);

void qb_send_message_sync(qbMessage* e);

struct qbChannel* qb_open_channel(const char* program, const char* event);
void qb_close_channel(struct qbChannel* channel);

// Subscribe a system to receive messages from an event
struct qbSubscription* qb_subscribe_to(const char* program, const char* event,
                                       struct qbSystem* system);
// Ubsubscribe a system from receiving events
void qb_unsubscribe_from(struct qbSubscription* subscription);

END_EXTERN_C

#endif  // #ifndef CUBEZ__H
