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

typedef struct qbEntity_* qbEntity;
typedef struct qbEntityAttr_* qbEntityAttr;
typedef struct qbComponent_* qbComponent;
typedef struct qbComponentAttr_* qbComponentAttr;
typedef struct qbSystem_* qbSystem;
typedef struct qbSystemAttr_* qbSystemAttr;
typedef struct qbCollectionAttr_* qbCollectionAttr;
typedef struct qbCollection_* qbCollection;

///////////////////////////////////////////////////////////
//////////////////////  Collections  //////////////////////
///////////////////////////////////////////////////////////

typedef struct qbElement_* qbElement;

qbId qb_element_getid(qbElement element);
qbResult qb_element_read(qbElement element, void* buffer);
qbResult qb_element_write(qbElement element);

struct qbCollectionInterface;

typedef qbHandle (*qbInsert)(qbCollectionInterface*, void* key, void* value);
typedef uint64_t (*qbCount)(qbCollectionInterface*);
typedef uint8_t* (*qbData)(qbCollectionInterface*);
typedef void* (*qbValueByOffset)(qbCollectionInterface*, uint64_t offset);
typedef void* (*qbValueByHandle)(qbCollectionInterface*, qbHandle handle);
typedef void* (*qbValueById)(qbCollectionInterface*, qbId entity_id);
typedef void (*qbRemoveByOffset)(qbCollectionInterface*, uint64_t offset);
typedef void (*qbRemoveByHandle)(qbCollectionInterface*, qbHandle handle);
typedef void (*qbRemoveById)(qbCollectionInterface*, qbId entity_id);

struct qbCollectionInterface {
  void* collection;

  qbInsert insert;

  qbValueByOffset by_offset;
  qbValueById by_id;
  qbValueByHandle by_handle;

  qbRemoveByOffset remove_by_offset;
  qbRemoveByHandle remove_by_handle;
  qbRemoveById remove_by_id;
};

qbResult qb_collectionattr_create(qbCollectionAttr* attr);
qbResult qb_collectionattr_destroy(qbCollectionAttr* attr);
qbResult qb_collectionattr_setprogram(qbCollectionAttr attr, const char* program);
qbResult qb_collectionattr_setremovers(qbCollectionAttr attr, qbRemoveByOffset,
                                       qbRemoveById, qbRemoveByHandle);
qbResult qb_collectionattr_setaccessors(qbCollectionAttr attr, qbValueByOffset,
                                        qbValueById, qbValueByHandle);
qbResult qb_collectionattr_setkeyiterator(qbCollectionAttr attr, qbData,
                                          size_t stride, uint32_t offset);
qbResult qb_collectionattr_setvalueiterator(qbCollectionAttr attr, qbData,
                                            size_t size, size_t stride,
                                            uint32_t offset);
qbResult qb_collectionattr_setinsert(qbCollectionAttr attr, qbInsert);
qbResult qb_collectionattr_setcount(qbCollectionAttr attr, qbCount);
qbResult qb_collectionattr_setimplementation(qbCollectionAttr attr, void* impl);

qbResult qb_collection_create(qbCollection* collection, qbCollectionAttr attr);
qbResult qb_collection_destroy(qbCollection* collection);
qbResult qb_collection_share(qbCollection collection, qbProgram destination);

///////////////////////////////////////////////////////////
////////////////////////  Systems  ////////////////////////
///////////////////////////////////////////////////////////

const int16_t QB_MAX_PRIORITY = 0x7FFF;
const int16_t QB_MIN_PRIORITY = 0x8001;

enum class qbTrigger {
  LOOP = 0,
  EVENT,
};

struct qbFrame {
  void* event;
  void* state;
};

typedef bool (*qbRunCondition)(qbCollectionInterface* sources,
                               qbCollectionInterface* sinks,
                               qbFrame* frame);

typedef void (*qbTransform)(qbElement* elements,
                            qbCollectionInterface* sinks, qbFrame* frame);

typedef void (*qbCallback)(qbCollectionInterface* sinks, qbFrame* frame);

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

// Relinquish ownership to engine and free it
//
// Arguments:
//   system The system to free
//
// Returns:
//   QB_OK on success

// Copies a system and its configuration to a different program
//
// Arguments:
//   source_system The system to copy
//   destination_program The program to copy the system to
//
// Returns:
//   Pointer to newly allocated system

// Enable a system to run.
//
// Arguments:
//   system The system to enable
//   policy The new policy to set
//
// Returns:
//   QB_OK on success

// Disable a system from running
//
// Arguments:
//   system The system to disable
//
// Returns:
//   QB_OK on success
//qbResult qb_disable_system(qbSystem system);

///////////////////////////////////////////////////////////
/////////////////////  Type Management  ///////////////////
///////////////////////////////////////////////////////////


qbResult qb_componentattr_create(qbComponentAttr* attr);
qbResult qb_componentattr_destroy(qbComponentAttr* attr);
qbResult qb_componentattr_setdatasize(qbComponentAttr attr, size_t size);
#define qb_componentattr_setdatatype(attr, type) \
    qb_componentattr_setdatasize(attr, sizeof(type))

qbResult qb_componentattr_setprogram(qbComponentAttr attr, const char* program);
qbResult qb_componentattr_setimplementation(qbComponentAttr attr,
                                            qbCollection* collection);

qbResult qb_component_create(qbComponent* component, qbComponentAttr attr);
qbResult qb_component_destroy(qbComponent* component);

qbResult qb_entityattr_create(qbEntityAttr* attr);
qbResult qb_entityattr_destroy(qbEntityAttr* attr);

qbResult qb_entityattr_addcomponent(qbEntityAttr attr, qbComponent component,
                                    void* instance_data);

qbResult qb_entity_create(qbEntity* entity, qbEntityAttr attr);
qbResult qb_entity_destroy(qbEntity* entity);
qbResult qb_entity_addcomponent(qbEntity entity, qbComponent component,
                                void* instance_data);
qbResult qb_entity_removecomponent(qbEntity entity, qbComponent component);
qbId qb_entity_getid(qbEntity entity);
qbResult qb_entity_find(qbEntity* entity, qbId entity_id);

enum qbComponentJoin {
  QB_JOIN_INNER = 0,
  QB_JOIN_LEFT,
  QB_JOIN_CROSS,
};

qbResult qb_systemattr_create(qbSystemAttr* attr);
qbResult qb_systemattr_destroy(qbSystemAttr* attr);
qbResult qb_systemattr_setprogram(qbSystemAttr attr, const char* program);
qbResult qb_systemattr_addsource(qbSystemAttr attr, qbComponent component);
qbResult qb_systemattr_addsink(qbSystemAttr attr, qbComponent component);
qbResult qb_systemattr_setfunction(qbSystemAttr attr, qbTransform transform);
qbResult qb_systemattr_setcallback(qbSystemAttr attr, qbCallback callback);

qbResult qb_systemattr_settrigger(qbSystemAttr attr, qbTrigger trigger);
qbResult qb_systemattr_setpriority(qbSystemAttr attr, int16_t priority);
qbResult qb_systemattr_setjoin(qbSystemAttr attr, qbComponentJoin join);
qbResult qb_systemattr_setuserstate(qbSystemAttr attr, void* state);

qbResult qb_system_create(qbSystem* system, qbSystemAttr attr);
qbResult qb_system_destroy(qbSystem* system);
qbResult qb_system_enable(qbSystem system);
qbResult qb_system_disable(qbSystem system);



///////////////////////////////////////////////////////////
//////////////////  Events and Messaging  /////////////////
///////////////////////////////////////////////////////////

typedef struct qbEventAttr_* qbEventAttr;
typedef struct qbEvent_* qbEvent;

qbResult qb_eventattr_create(qbEventAttr* attr);
qbResult qb_eventattr_destroy(qbEventAttr* attr);
qbResult qb_eventattr_setprogram(qbEventAttr attr, const char* program);
qbResult qb_eventattr_setmessagesize(qbEventAttr attr, size_t size);
#define qb_eventattr_setmessagetype(attr, type) \
    qb_eventattr_setmessagesize(attr, sizeof(type))

// Thread-safe
qbResult qb_event_create(qbEvent* event, qbEventAttr attr);
qbResult qb_event_destroy(qbEvent* event);

// Flush events from program. If event if null, flush all events from program.
// Engine must not be in the LOOPING run state.
qbResult qb_event_flush(qbEvent event);
qbResult qb_event_flushall(qbProgram program);
qbResult qb_event_subscribe(qbEvent event, qbSystem system);
qbResult qb_event_unsubscribe(qbEvent event, qbSystem system);

// Thread-safe.
qbResult qb_event_send(qbEvent event, void* message);
qbResult qb_event_sendsync(qbEvent event, void* message);

END_EXTERN_C

#endif  // #ifndef CUBEZ__H
