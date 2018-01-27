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
typedef struct qbInstance_* qbInstance;
typedef struct qbEventAttr_* qbEventAttr;
typedef struct qbEvent_* qbEvent;

///////////////////////////////////////////////////////////
///////////////////////  Components  //////////////////////
///////////////////////////////////////////////////////////

DLLEXPORT qbResult qb_componentattr_create(qbComponentAttr* attr);
DLLEXPORT qbResult qb_componentattr_destroy(qbComponentAttr* attr);
DLLEXPORT qbResult qb_componentattr_setdatasize(qbComponentAttr attr, size_t size);
#define qb_componentattr_setdatatype(attr, type) \
    qb_componentattr_setdatasize(attr, sizeof(type))

DLLEXPORT qbResult qb_componentattr_setprogram(qbComponentAttr attr, qbId program);

DLLEXPORT qbResult qb_component_create(qbComponent* component, qbComponentAttr attr);
DLLEXPORT qbResult qb_component_destroy(qbComponent* component);

DLLEXPORT size_t qb_component_getcount(qbComponent component);

///////////////////////////////////////////////////////////
////////////////////////  Instances  //////////////////////
///////////////////////////////////////////////////////////

// Triggers fn when a component is created. If created when an entity is
// created, then it is triggered after all components have been instantiated.
DLLEXPORT qbResult qb_instance_oncreate(qbComponent component,
                                        void(*fn)(qbInstance instance));

// Triggers fn when a component is destroyed. Is triggered before before memory
// is freed.
DLLEXPORT qbResult qb_instance_ondestroy(qbComponent component,
                                         void(*fn)(qbInstance instance));

DLLEXPORT qbEntity qb_instance_getentity(qbInstance instance);
DLLEXPORT qbResult qb_instance_getconst(qbInstance instance, void* pbuffer);
DLLEXPORT qbResult qb_instance_getmutable(qbInstance instance, void* pbuffer);
DLLEXPORT qbResult qb_instance_getcomponent(qbInstance instance, qbComponent component, void* pbuffer);
DLLEXPORT bool qb_instance_hascomponent(qbInstance instance, qbComponent component);

///////////////////////////////////////////////////////////
////////////////////////  Entities  ///////////////////////
///////////////////////////////////////////////////////////
DLLEXPORT qbResult qb_entityattr_create(qbEntityAttr* attr);
DLLEXPORT qbResult qb_entityattr_destroy(qbEntityAttr* attr);

DLLEXPORT qbResult qb_entityattr_addcomponent(qbEntityAttr attr, qbComponent component,
                                    void* instance_data);

DLLEXPORT qbResult qb_entity_create(qbEntity* entity, qbEntityAttr attr);
DLLEXPORT qbResult qb_entity_destroy(qbEntity entity);

DLLEXPORT qbResult qb_entity_addcomponent(qbEntity entity, qbComponent component,
                                          void* instance_data);
DLLEXPORT qbResult qb_entity_removecomponent(qbEntity entity, qbComponent component);
DLLEXPORT bool qb_entity_hascomponent(qbEntity entity, qbComponent component);

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
  qbSystem system;
  void* event;
  void* state;
};

enum qbComponentJoin {
  QB_JOIN_INNER = 0,
  QB_JOIN_LEFT,
  QB_JOIN_CROSS,
};

typedef void (*qbTransform)(qbInstance* instances, qbFrame* frame);

typedef void (*qbCallback)(qbFrame* frame);

DLLEXPORT qbResult qb_systemattr_create(qbSystemAttr* attr);
DLLEXPORT qbResult qb_systemattr_destroy(qbSystemAttr* attr);
DLLEXPORT qbResult qb_systemattr_setprogram(qbSystemAttr attr, qbId program);

DLLEXPORT qbResult qb_systemattr_addconst(qbSystemAttr attr, qbComponent component);
DLLEXPORT qbResult qb_systemattr_addmutable(qbSystemAttr attr, qbComponent component);

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
DLLEXPORT qbResult qb_eventattr_setprogram(qbEventAttr attr, qbId program);
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

// Thread-safe.
DLLEXPORT qbResult qb_event_send(qbEvent event, void* message);
DLLEXPORT qbResult qb_event_sendsync(qbEvent event, void* message);

END_EXTERN_C

#endif  // #ifndef CUBEZ__H
