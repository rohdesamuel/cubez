/**
* Author: Samuel Rohde (rohde.samuel@gmail.com)
*
* This file is subject to the terms and conditions defined in
* file 'LICENSE.txt', which is part of this source code package.
*/

#ifndef CUBEZ__H
#define CUBEZ__H

#include "common.h"

///////////////////////////////////////////////////////////
//////////////////////  Flow Control  /////////////////////
///////////////////////////////////////////////////////////

// Holds the game engine state
typedef struct {
  void* self;
} qbUniverse;

API qbResult qb_init(qbUniverse* universe);
API qbResult qb_start();
API qbResult qb_stop();
API qbResult qb_loop();

// ======== qbProgram ========
// A program is a structure that holds all encapsulated state. This includes
// collections, systems, events, and subscriptions. Programs can only talk to
// each other through messages. Programs can access each other's collections
// only after it has been shared.
typedef struct {
  const qbId id;
  const char* name;
  const void* self;
} qbProgram;

// Creates a program with the specified name. Copies the name into new memory.
API qbId qb_create_program(const char* name);

// Runs a particular program flushing its events then running all of its systems.
API qbResult qb_run_program(qbId program);

// Detaches a program from the main game loop. This starts an asynchronous
// thread.
API qbResult qb_detach_program(qbId program);

// Joins a program with the main game loop.
API qbResult qb_join_program(qbId program);

typedef qbId qbEntity;
typedef struct qbEntityAttr_* qbEntityAttr;
typedef qbId qbComponent;
typedef struct qbComponentAttr_* qbComponentAttr;
typedef struct qbSystem_* qbSystem;
typedef struct qbSystemAttr_* qbSystemAttr;
typedef struct qbElement_* qbElement;
typedef struct qbInstance_* qbInstance;
typedef struct qbEventAttr_* qbEventAttr;
typedef struct qbEvent_* qbEvent;
typedef struct qbStream_* qbStream;
typedef struct qbBarrier_* qbBarrier;
typedef struct qbBarrierOrder_* qbBarrierOrder;
typedef struct qbScene_* qbScene;
typedef struct qbCoro_* qbCoro;
typedef struct qbAsync_* qbAsync;
typedef struct qbAlarm_* qbAlarm;

///////////////////////////////////////////////////////////
///////////////////////  Components  //////////////////////
///////////////////////////////////////////////////////////

// ======== qbComponentType ========
typedef enum {
  // A piece of serializable memory.
  QB_COMPONENT_TYPE_RAW = 0,

  // A pointer to a piece of memory. Will be freed when instance is destroyed.
  // Pointer must not be allocated with new or new[] operators.
  QB_COMPONENT_TYPE_POINTER,

  // A struct only comprised of "qbEntity"s as its members. Will destroy all
  // entities inside of the struct.
  QB_COMPONENT_TYPE_COMPOSITE,
} qbComponentType;

// ======== qbComponentAttr ========
// Creates a new qbComponentAttr object for qbComponent creation.
API qbResult      qb_componentattr_create(qbComponentAttr* attr);

// Destroys the specified attribute.
API qbResult      qb_componentattr_destroy(qbComponentAttr* attr);

// Sets the allocated size for the component.
API qbResult      qb_componentattr_setdatasize(qbComponentAttr attr,
                                               size_t size);

// Sets the component type which tells the engine how to destroy the component.
API qbResult      qb_componentattr_settype(qbComponentAttr attr,
                                           qbComponentType type);

// Sets the component to be shared across programs with a reader/writer lock.
API qbResult      qb_componentattr_setshared(qbComponentAttr attr);

// Sets the data type for the component.
// Same as qb_componentattr_setdatasize(attr, sizeof(type)).
#define qb_componentattr_setdatatype(attr, type) \
    qb_componentattr_setdatasize(attr, sizeof(type))

// ======== qbComponent ========
// Creates a new qbComponent with the specified attributes.
API qbResult      qb_component_create(qbComponent* component,
                                            qbComponentAttr attr);

// Destroys the specified qbComponent.
API qbResult      qb_component_destroy(qbComponent* component);

// Returns the number of specified components.
API size_t        qb_component_getcount(qbComponent component);

///////////////////////////////////////////////////////////
////////////////////////  Instances  //////////////////////
///////////////////////////////////////////////////////////

// ======== qbInstance ========
// A qbInstance is a per-program and per-thread (not thread-safe) entity
// combined with its component references.

// Triggers fn when a instance of the given component is created. If created
// when an entity is created, then it is triggered after all components have
// been instantiated.
API qbResult      qb_instance_oncreate(qbComponent component,
                                       void(*fn)(qbInstance instance));

// Triggers fn when a instance of the given component is destroyed. Is
// triggered before before memory is freed.
API qbResult      qb_instance_ondestroy(qbComponent component,
                                        void(*fn)(qbInstance instance));

// Gets a read-only view of a component instance for the given entity.
API qbResult      qb_instance_find(qbComponent component,
                                   qbEntity entity,
                                   void* pbuffer);

// Returns the entity that contains this component instance.
API qbEntity      qb_instance_getentity(qbInstance instance);

// Fills pbuffer with component instance data. If the parent component is
// shared, this locks the reader lock.
API qbResult      qb_instance_getconst(qbInstance instance,
                                       void* pbuffer);

// Fills pbuffer with component instance data. If the parent component is
// shared, this locks the writer lock.
API qbResult     qb_instance_getmutable(qbInstance instance,
                                        void* pbuffer);

// Fills pbuffer with component instance data. The memory is mutable.
API qbResult     qb_instance_getcomponent(qbInstance instance,
                                          qbComponent component,
                                          void* pbuffer);

// Returns true if the entity containing the instance also contains a specified
// component.
API bool        qb_instance_hascomponent(qbInstance instance,
                                         qbComponent component);

///////////////////////////////////////////////////////////
////////////////////////  Entities  ///////////////////////
///////////////////////////////////////////////////////////

// ======== qbEntityAttr ========
// Creates a new qbEntityAttr object for entity creation.
API qbResult      qb_entityattr_create(qbEntityAttr* attr);

// Destroys the specified attribute.
API qbResult      qb_entityattr_destroy(qbEntityAttr* attr);

// Adds a component with instance data to be copied into the entity.
// This only copies the instance_data pointer and does not allocate new memory.
API qbResult      qb_entityattr_addcomponent(qbEntityAttr attr,
                                             qbComponent component,
                                             void* instance_data);
// ======== qbEntity ========
// A qbEntity is an identifier to a game object. qbComponents can be added to
// the entity.
// Creates a new qbEntity with the specified attributes.
API qbResult      qb_entity_create(qbEntity* entity,
                                   qbEntityAttr attr);

// Destroys the specified entity.
API qbResult      qb_entity_destroy(qbEntity entity);

// Adds a component with instance data to copied to the entity.
// This allocates a new instance copies the instance_data to the newly
// allocated memory. This calls the instance's OnCreate function immediately.
API qbResult      qb_entity_addcomponent(qbEntity entity,
                                         qbComponent component,
                                         void* instance_data);

// Removes the specified component from the entity. Does not remove the
// component until after the current frame has completed. This calls the
// instance's OnDestroy function after the current frame has completed.
API qbResult      qb_entity_removecomponent(qbEntity entity,
                                            qbComponent component);

// Fills instance with a reference to the component instance data for the
// specified entity and component.
API qbResult      qb_entity_getinstance(qbEntity entity,
                                        qbComponent component,
                                        qbInstance* instance);

// Returns true if the specified entity contains an instance for the component.
API bool          qb_entity_hascomponent(qbEntity entity,
                                         qbComponent component);

///////////////////////////////////////////////////////////
////////////////////////  Systems  ////////////////////////
///////////////////////////////////////////////////////////

// ======== qbFrame ========
// a qbFrame is a struct that is filled in during execution time. If the system
// was triggered by an event, the "event" member will point to its message. If
// the system has user state, defined with "setuserstate" this will be filled
// in.
typedef struct {
  qbSystem system;
  void* event;
  void* state;
} qbFrame;

// ======== qbBarrier ========
// A barrier is a synchronization object that enforces order between systems
// that run on different programs. The first system to use the "addbarrier"
// function will be enforced to run first. All subsequent systems that use
// "addbarrier" will be dependent on the first to run.

// Creates a qbBarrier.
API qbResult      qb_barrier_create(qbBarrier* barrier);

// Destroys a barrier.
API qbResult      qb_barrier_destroy(qbBarrier* barrier);

// ======== qbSystemAttr ========
// Creates a new qbSystemAttr object for system creation.
API qbResult      qb_systemattr_create(qbSystemAttr* attr);

// Destroys the specified attribute.
API qbResult      qb_systemattr_destroy(qbSystemAttr* attr);

// Adds a read-only component to the be read when the system is run.
API qbResult      qb_systemattr_addconst(qbSystemAttr attr,
                                         qbComponent component);

// Adds a mutable component to the be read when the system is run.
API qbResult      qb_systemattr_addmutable(qbSystemAttr attr,
                                           qbComponent component);

// ======== qbComponentJoin ========
typedef enum {
  QB_JOIN_INNER = 0,
  QB_JOIN_LEFT,
  QB_JOIN_CROSS,
} qbComponentJoin;

// Instructs the execution of the system to join together multiple components.
API qbResult      qb_systemattr_setjoin(qbSystemAttr attr,
                                        qbComponentJoin join);

// Sets the program where the system will be run. By default, the system is run
// on the same thread as "qb_loop()".
API qbResult      qb_systemattr_setprogram(qbSystemAttr attr,
                                           qbId program);

// Sets the transform to run during execution. The specified transform will be
// run on every component instance that was added with "addconst" and
// "addmutable".
typedef void(*qbTransform)(qbInstance* instances, qbFrame* frame);
API qbResult      qb_systemattr_setfunction(qbSystemAttr attr,
                                            qbTransform transform);

// Sets the callback to run after the system finishes executing its transform
// over all of its components.
typedef void(*qbCallback)(qbFrame* frame);
API qbResult      qb_systemattr_setcallback(qbSystemAttr attr,
                                            qbCallback callback);

// Allows the system to execute if the specified condition returns true.
typedef bool(*qbCondition)(qbFrame* frame);
API qbResult      qb_systemattr_setcondition(qbSystemAttr attr,
                                             qbCondition condition);

// ======== qbTrigger ========
typedef enum {
  QB_TRIGGER_LOOP = 0,
  QB_TRIGGER_EVENT
} qbTrigger;
// Sets the trigger for the system. Systems by default are triggered by the
// main execution loop with "qb_loop()". To detach a system to only be run
// for an event, use the QB_TRIGGER_EVENT value.
API qbResult      qb_systemattr_settrigger(qbSystemAttr attr,
                                           qbTrigger trigger);

// ======== Priorities ========
const int16_t QB_MAX_PRIORITY = (int16_t)0x7FFF;
const int16_t QB_MIN_PRIORITY = (int16_t)0x8001;
// Sets the priority for the system. Systems with higher priority values will
// be run before systems with lower priorities.
API qbResult      qb_systemattr_setpriority(qbSystemAttr attr,
                                            int16_t priority);

// Adds a barrier to the system to enforce ordering across programs.
API qbResult      qb_systemattr_addbarrier(qbSystemAttr attr,
                                           qbBarrier barrier);

// Sets a pointer to be passed in with every execution of the system.
API qbResult      qb_systemattr_setuserstate(qbSystemAttr attr,
                                             void* state);

// ======== qbSystem ========
// A qbSystem is the atomic unit of synchronous execution. Systems are run when
// either: qb_loop() is called, a program is detached and runs continuously, or
// an event is triggered. Once a system is executed, it will iterate through
// all of its specified components and execute its transform over every
// instance.
// Creates a new qbSystem with the specified attributes.
API qbResult      qb_system_create(qbSystem* system,
                                   qbSystemAttr attr);

// Destroys the specified system.
API qbResult      qb_system_destroy(qbSystem* system);

// Enables the specified system and resume all execution.
API qbResult      qb_system_enable(qbSystem system);

// Disables the specified system and stop all execution.
API qbResult      qb_system_disable(qbSystem system);

// Runs the given system. Not thread-safe when run concurrently with qb_loop().
// Unimplemented.
API qbResult      qb_system_run(qbSystem system);

///////////////////////////////////////////////////////////
//////////////////  Events and Messaging  /////////////////
///////////////////////////////////////////////////////////

// ======== qbEventAttr ========
// Creates a new qbEventAttr object for event creation.
API qbResult      qb_eventattr_create(qbEventAttr* attr);

// Destroys the specified attributes.
API qbResult      qb_eventattr_destroy(qbEventAttr* attr);

// Sets which program to associate the event with. The event will only trigger
// inside the specified program.
API qbResult      qb_eventattr_setprogram(qbEventAttr attr,
                                          qbId program);

// Sets the size of each message to be allocated to send.
API qbResult      qb_eventattr_setmessagesize(qbEventAttr attr, size_t size);
#define qb_eventattr_setmessagetype(attr, type) \
    qb_eventattr_setmessagesize(attr, sizeof(type))

// ======== qbEvent ========
// A qbEvent is a way of passing messages between systems in a single program.
// Sending messages is not thread-safe.
// Creates a new qbEvent with the specified attributes.
API qbResult      qb_event_create(qbEvent* event,
                                  qbEventAttr attr);

// Destroys the specified event.
API qbResult      qb_event_destroy(qbEvent* event);

// Flushes all events from the specified program.
API qbResult      qb_event_flushall(qbProgram program);

// Subscribes the specified system to the event. This system will execute any
// time the event is triggered. The system must have a trigger of
// QB_TRIGGER_EVENT.
API qbResult      qb_event_subscribe(qbEvent event,
                                     qbSystem system);

// Unsubscribes the specified system from the event.
API qbResult      qb_event_unsubscribe(qbEvent event,
                                       qbSystem system);

// Sends a messages on the event. This triggers all subscribed systems before
// the next frame is run.
API qbResult      qb_event_send(qbEvent event,
                                void* message);

// Sends a messages on the event. This immediately triggers all subscribed
// systems.
API qbResult      qb_event_sendsync(qbEvent event,
                                    void* message);


///////////////////////////////////////////////////////////
/////////////////////////  Scenes  ////////////////////////
///////////////////////////////////////////////////////////

API qbResult      qb_scene_create(qbScene* scene,
                                  const char* name);

// Unimplemented.
API qbResult      qb_scene_save(qbScene* scene,
                                const char* file);

// Unimplemented.
API qbResult      qb_scene_load(qbScene* scene,
                                const char* name,
                                const char* file);

API qbResult      qb_scene_destroy(qbScene* scene);

// Returns the global scene singleton. This scene is created at the start of
// the engine. This is useful if there are resources or entities that are
// needed throughout the lifetime of the game.
API qbScene       qb_scene_global();

// Sets scene to be create entities and load resources on.
API qbResult      qb_scene_set(qbScene scene);

// Resets scene to currently active scene.
API qbResult      qb_scene_reset();

// Activates the given scene.
API qbResult      qb_scene_activate(qbScene scene);

API qbResult      qb_scene_ondestroy(qbScene scene,
                                     void(*fn)(qbScene scene));

API qbResult      qb_scene_onenable(qbScene scene,
                                    void(*fn)(qbScene scene));

API qbResult      qb_scene_ondisable(qbScene scene,
                                     void(*fn)(qbScene scene));


///////////////////////////////////////////////////////////
///////////////////////  Coroutines  //////////////////////
///////////////////////////////////////////////////////////

typedef enum {
  QB_TAG_VOID,
  QB_TAG_UINT,
  QB_TAG_INT,
  QB_TAG_DOUBLE,
  QB_TAG_CHAR,
  QB_TAG_UNSET,
} qbTag;

typedef struct {
  qbTag tag;
  union {
    void* p;
    uint64_t u;
    int64_t i;
    double d;
    char c;
  };
} qbVar;

API extern const qbVar qbNone;
API extern const qbVar qbUnset;

API qbVar       qbVoid(void* p);

API qbVar       qbUint(uint64_t u);

API qbVar       qbInt(int64_t i);

API qbVar       qbDouble(double d);

API qbVar       qbChar(char c);

// Creates and returns a new coroutine only valid on the current thread.
// Cannot be passed between threads.
API qbCoro      qb_coro_create(qbVar(*entry)(qbVar var));

// A coroutine is safe to destroy only it is finished running. This can be
// queried with qb_coro_peek or qb_coro_done. A coroutine can be waited upon by
// using qb_coro_await.
API qbResult    qb_coro_destroy(qbCoro* coro);

// Immediately runs the given coroutine on the same thread as the caller.
// WARNING: A coroutine has its own stack, do not pass in pointers to stack
// variables. They will be invalid pointers.
API qbVar       qb_coro_call(qbCoro coro, qbVar var);

// Creates a coroutine and schedules the given function to be run on the main
// thread. All coroutines are then run serially after event dispatch and
// systems are run. All coroutines are run as cooperative threads. In order to
// run the next coroutine, the given entry must call qb_coro_yield().
// WARNING: A coroutine has its own stack, do not pass in pointers to stack
// variables. They will be invalid pointers.
API qbCoro      qb_coro_sync(qbVar(*entry)(qbVar), qbVar var);

// Creates a coroutine and schedules the given function to be run on a
// background thread. Thread-safe.
// WARNING: A coroutine has its own stack, do not pass in pointers to stack
// variables. They will be invalid pointers.
API qbCoro      qb_coro_async(qbVar(*entry)(qbVar), qbVar var);

// Yields control with the given var back to the current coroutine's caller.
// WARNING: A coroutine has its own stack, do not pass in pointers to stack
// variables. They will be invalid pointers.
API qbVar       qb_coro_yield(qbVar var);

// Yields "qbUnset" until at least the given seconds have elapsed.
API void        qb_coro_wait(double seconds);

// Yields "qbUnset" until the given frames have elapsed.
API void        qb_coro_waitframes(uint32_t frames);

// Yields "qbUnset" until coro is done running. 
API qbVar       qb_coro_await(qbCoro coro);

// Peeks at the return value of the scheduled coro. Returns "qbUnset" if the
// scheduled coro is running.
API qbVar       qb_coro_peek(qbCoro coro);

// Returns true if Coroutine is finished running.
API bool        qb_coro_done(qbCoro coro);

#endif  // #ifndef CUBEZ__H
