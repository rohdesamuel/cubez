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

DLLEXPORT qbResult qb_init(struct qbUniverse* universe);
DLLEXPORT qbResult qb_start();
DLLEXPORT qbResult qb_stop();
DLLEXPORT qbResult qb_loop();

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
DLLEXPORT qbId qb_create_program(const char* name);

// Runs a particular program flushing its events and running all of its systems
//
// Arguments:
//   program The program Id to flush
//
// Returns:
//   QB_OK on success
DLLEXPORT qbResult qb_run_program(qbId program);

// Detaches a program from the main game loop. This starts an asynchronous
// thread.
//
// Arguments:
//   program The program Id to detach
//
// Returns:
//   QB_OK on success
DLLEXPORT qbResult qb_detach_program(qbId program);

// Joins a program with the main game loop.
//
// Arguments:
//   program The program Id to join
//
// Returns:
//   QB_OK on success
DLLEXPORT qbResult qb_join_program(qbId program);

typedef qbId qbEntity;
typedef struct qbEntityAttr_* qbEntityAttr;
typedef struct qbComponent_* qbComponent;
typedef struct qbComponentAttr_* qbComponentAttr;
typedef struct qbSystem_* qbSystem;
typedef struct qbSystemAttr_* qbSystemAttr;
typedef struct qbElement_* qbElement;
typedef struct qbEventAttr_* qbEventAttr;
typedef struct qbEvent_* qbEvent;
typedef struct qbCollectionAttr_* qbCollectionAttr;
typedef struct qbCollection_* qbCollection;

typedef qbHandle (*qbInsert)(struct qbCollectionInterface*, void* key, void* value);
typedef uint64_t (*qbCount)(struct qbCollectionInterface*);
typedef uint8_t* (*qbData)(struct qbCollectionInterface*);
typedef void* (*qbValueByOffset)(struct qbCollectionInterface*, uint64_t offset);
typedef void* (*qbValueByHandle)(struct qbCollectionInterface*, qbHandle handle);
typedef void* (*qbValueById)(struct qbCollectionInterface*, qbId entity_id);
typedef void (*qbRemoveByOffset)(struct qbCollectionInterface*, uint64_t offset);
typedef void (*qbRemoveByHandle)(struct qbCollectionInterface*, qbHandle handle);
typedef void (*qbRemoveById)(struct qbCollectionInterface*, qbId entity_id);

struct qbCollectionInterface {
  void* collection;

  qbInsert insert;

  qbValueByOffset by_offset;
  qbValueById by_id;

  qbRemoveByOffset remove_by_offset;
  qbRemoveById remove_by_id;
};

///////////////////////////////////////////////////////////
///////////////////////  Components  //////////////////////
///////////////////////////////////////////////////////////

DLLEXPORT qbResult qb_componentattr_create(qbComponentAttr* attr);
DLLEXPORT qbResult qb_componentattr_destroy(qbComponentAttr* attr);
DLLEXPORT qbResult qb_componentattr_setdatasize(qbComponentAttr attr, size_t size);
#define qb_componentattr_setdatatype(attr, type) \
    qb_componentattr_setdatasize(attr, sizeof(type))

DLLEXPORT qbResult qb_componentattr_setprogram(qbComponentAttr attr, const char* program);

DLLEXPORT qbResult qb_component_create(qbComponent* component, qbComponentAttr attr);
DLLEXPORT qbResult qb_component_destroy(qbComponent* component);

DLLEXPORT size_t qb_component_getcount(qbComponent component);

// Triggers fn when a component is created. If created when an entity is
// created, then it is triggered after all components have been instantiated.
DLLEXPORT qbResult qb_component_oncreate(qbComponent component,
    void(*fn)(qbEntity parent_entity, qbComponent component, void* instance_data));

// Triggers fn when a component is destroyed. Is triggered before before memory
// is freed.
DLLEXPORT qbResult qb_component_ondestroy(qbComponent component,
    void(*fn)(qbEntity parent_entity, qbComponent component, void* instance_data));

///////////////////////////////////////////////////////////
////////////////////////  Entities  ///////////////////////
///////////////////////////////////////////////////////////
DLLEXPORT qbResult qb_entityattr_create(qbEntityAttr* attr);
DLLEXPORT qbResult qb_entityattr_destroy(qbEntityAttr* attr);

DLLEXPORT qbResult qb_entityattr_addcomponent(qbEntityAttr attr, qbComponent component,
                                    void* instance_data);

DLLEXPORT qbResult qb_entity_create(qbEntity* entity, qbEntityAttr attr);
DLLEXPORT qbResult qb_entity_destroy(qbEntity* entity);
DLLEXPORT qbResult qb_entity_find(qbEntity* entity, qbId entity_id);
DLLEXPORT qbResult qb_entity_getcomponent(qbEntity entity, qbComponent component, void* buffer);

DLLEXPORT qbResult qb_entity_hascomponent(qbEntity entity, qbComponent component);

DLLEXPORT qbResult qb_entity_addcomponent(qbEntity entity, qbComponent component,
                                void* instance_data);
DLLEXPORT qbResult qb_entity_removecomponent(qbEntity entity, qbComponent component);
DLLEXPORT qbId qb_entity_getid(qbEntity entity);

///////////////////////////////////////////////////////////
////////////////////////  Systems  ////////////////////////
///////////////////////////////////////////////////////////

const int16_t QB_MAX_PRIORITY = (int16_t)0x7FFF;
const int16_t QB_MIN_PRIORITY = (int16_t)0x8001;

enum qbTrigger {
  QB_TRIGGER_LOOP = 0,
  QB_TRIGGER_EVENT,
};

struct qbFrame {
  void* event;
  void* state;
};

enum qbComponentJoin {
  QB_JOIN_INNER = 0,
  QB_JOIN_LEFT,
  QB_JOIN_CROSS,
};

typedef void (*qbTransform)(qbElement* elements, qbFrame* frame);

typedef void (*qbCallback)(qbFrame* frame);

DLLEXPORT qbResult qb_systemattr_create(qbSystemAttr* attr);
DLLEXPORT qbResult qb_systemattr_destroy(qbSystemAttr* attr);
DLLEXPORT qbResult qb_systemattr_setprogram(qbSystemAttr attr, const char* program);
DLLEXPORT qbResult qb_systemattr_addsource(qbSystemAttr attr, qbComponent component);
DLLEXPORT qbResult qb_systemattr_addsink(qbSystemAttr attr, qbComponent component);
DLLEXPORT qbResult qb_systemattr_setfunction(qbSystemAttr attr, qbTransform transform);
DLLEXPORT qbResult qb_systemattr_setcallback(qbSystemAttr attr, qbCallback callback);

DLLEXPORT qbResult qb_systemattr_settrigger(qbSystemAttr attr, qbTrigger trigger);
DLLEXPORT qbResult qb_systemattr_setpriority(qbSystemAttr attr, int16_t priority);
DLLEXPORT qbResult qb_systemattr_setjoin(qbSystemAttr attr, qbComponentJoin join);
DLLEXPORT qbResult qb_systemattr_setuserstate(qbSystemAttr attr, void* state);

DLLEXPORT qbResult qb_system_create(qbSystem* system, qbSystemAttr attr);
DLLEXPORT qbResult qb_system_destroy(qbSystem* system);
DLLEXPORT qbResult qb_system_enable(qbSystem system);
DLLEXPORT qbResult qb_system_disable(qbSystem system);

///////////////////////////////////////////////////////////
//////////////////  Events and Messaging  /////////////////
///////////////////////////////////////////////////////////

DLLEXPORT qbResult qb_eventattr_create(qbEventAttr* attr);
DLLEXPORT qbResult qb_eventattr_destroy(qbEventAttr* attr);
DLLEXPORT qbResult qb_eventattr_setprogram(qbEventAttr attr, const char* program);
DLLEXPORT qbResult qb_eventattr_setmessagesize(qbEventAttr attr, size_t size);
#define qb_eventattr_setmessagetype(attr, type) \
    qb_eventattr_setmessagesize(attr, sizeof(type))

// Thread-safe
DLLEXPORT qbResult qb_event_create(qbEvent* event, qbEventAttr attr);
DLLEXPORT qbResult qb_event_destroy(qbEvent* event);

// Flush events from program. If event is null, flush all events from program.
// Engine must not be in the LOOPING run state.
DLLEXPORT qbResult qb_event_flushall(qbProgram program);
DLLEXPORT qbResult qb_event_subscribe(qbEvent event, qbSystem system);
DLLEXPORT qbResult qb_event_unsubscribe(qbEvent event, qbSystem system);

DLLEXPORT qbResult qb_event_subscribewith(qbEvent event, qbSystem system,
                                qbComponent component);

// Thread-safe.
DLLEXPORT qbResult qb_event_sendto(qbEvent event, qbComponent component, void* message);
DLLEXPORT qbResult qb_event_send(qbEvent event, void* message);
DLLEXPORT qbResult qb_event_sendsync(qbEvent event, void* message);

///////////////////////////////////////////////////////////
//////////////////////  Collections  //////////////////////
///////////////////////////////////////////////////////////

DLLEXPORT qbId qb_element_getid(qbElement element);
DLLEXPORT qbEntity qb_element_getentity(qbElement element);
DLLEXPORT qbResult qb_element_read(qbElement element, void* buffer);
DLLEXPORT qbResult qb_element_write(qbElement element);

DLLEXPORT qbResult qb_collectionattr_create(qbCollectionAttr* attr);
DLLEXPORT qbResult qb_collectionattr_destroy(qbCollectionAttr* attr);
DLLEXPORT qbResult qb_collectionattr_setprogram(qbCollectionAttr attr, const char* program);
DLLEXPORT qbResult qb_collectionattr_setremovers(qbCollectionAttr attr, qbRemoveByOffset,
                                       qbRemoveById, qbRemoveByHandle);
DLLEXPORT qbResult qb_collectionattr_setaccessors(qbCollectionAttr attr, qbValueByOffset,
                                        qbValueById, qbValueByHandle);
DLLEXPORT qbResult qb_collectionattr_setkeyiterator(qbCollectionAttr attr, qbData,
                                          size_t stride, uint32_t offset);
DLLEXPORT qbResult qb_collectionattr_setvalueiterator(qbCollectionAttr attr, qbData,
                                            size_t size, size_t stride,
                                            uint32_t offset);
DLLEXPORT qbResult qb_collectionattr_setinsert(qbCollectionAttr attr, qbInsert);
DLLEXPORT qbResult qb_collectionattr_setcount(qbCollectionAttr attr, qbCount);
DLLEXPORT qbResult qb_collectionattr_setimplementation(qbCollectionAttr attr, void* impl);

DLLEXPORT qbResult qb_collection_create(qbCollection* collection, qbCollectionAttr attr);
DLLEXPORT qbResult qb_collection_destroy(qbCollection* collection);
DLLEXPORT qbResult qb_collection_share(qbCollection collection, qbProgram destination);


END_EXTERN_C

#endif  // #ifndef CUBEZ__H
