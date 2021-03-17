/**
* Author: Samuel Rohde (rohde.samuel@cubez.io)
*
* Copyright 2020 Samuel Rohde
* 
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#ifndef CUBEZ__H
#define CUBEZ__H

#define QB_MAJOR_VERSION 0
#define QB_MINOR_VERSION 0
#define QB_PATCH_VERSION 0

#include "common.h"

typedef enum {
  QB_TAG_CUSTOM = -1,
  QB_TAG_NIL = 0,
  QB_TAG_FUTURE,
  QB_TAG_ANY,

  // Scalar types.
  QB_TAG_PTR,
  QB_TAG_INT,
  QB_TAG_UINT,
  QB_TAG_DOUBLE,
  QB_TAG_STRING,
  QB_TAG_CSTRING,

  // Container types.
  QB_TAG_STRUCT,
  QB_TAG_ARRAY,
  QB_TAG_MAP,
  
  // Buffer type.
  QB_TAG_BYTES,
} qbTag;

typedef struct {
  const size_t capacity;  
  uint8_t* bytes;
} qbBuffer_;

QB_API size_t qb_buffer_write(qbBuffer_* buf, ptrdiff_t* pos, size_t size, const void* bytes);
QB_API size_t qb_buffer_read(const qbBuffer_* buf, ptrdiff_t* pos, size_t size, void* bytes);

QB_API size_t qb_buffer_writestr(qbBuffer_* buf, ptrdiff_t* pos, size_t len, const char* s);
QB_API size_t qb_buffer_writell(qbBuffer_* buf, ptrdiff_t* pos, int64_t n);
QB_API size_t qb_buffer_writel(qbBuffer_* buf, ptrdiff_t* pos, int32_t n);
QB_API size_t qb_buffer_writes(qbBuffer_* buf, ptrdiff_t* pos, int16_t n);
QB_API size_t qb_buffer_writed(qbBuffer_* buf, ptrdiff_t* pos, double n);
QB_API size_t qb_buffer_writef(qbBuffer_* buf, ptrdiff_t* pos, float n);

QB_API size_t qb_buffer_readstr(const qbBuffer_* buf, ptrdiff_t* pos, size_t* len, char** s);
QB_API size_t qb_buffer_readll(const qbBuffer_* buf, ptrdiff_t* pos, int64_t* n);
QB_API size_t qb_buffer_readl(const qbBuffer_* buf, ptrdiff_t* pos, int32_t* n);
QB_API size_t qb_buffer_reads(const qbBuffer_* buf, ptrdiff_t* pos, int16_t* n);
QB_API size_t qb_buffer_readd(const qbBuffer_* buf, ptrdiff_t* pos, double* n);
QB_API size_t qb_buffer_readf(const qbBuffer_* buf, ptrdiff_t* pos, float* n);

typedef struct {
  qbTag tag;
  size_t size;

  union {
    void* p;
    utf8_t* s;
    int64_t i;
    uint64_t u;
    double d;

    char bytes[8];
  };
} qbVar;

typedef qbVar* qbRef;
typedef struct qbSchema_* qbSchema;
typedef struct qbSchemaField_* qbSchemaField;

// Convenience functions for creating a qbVar. 
QB_API qbVar       qbPtr(void* p);
QB_API qbVar       qbInt(int64_t i);
QB_API qbVar       qbUint(uint64_t u);
QB_API qbVar       qbDouble(double d);
QB_API qbVar       qbStruct(qbSchema schema, void* buf);
QB_API qbVar       qbArray(qbTag v_type);
QB_API qbVar       qbMap(qbTag k_type, qbTag v_type);
QB_API qbVar       qbString(const utf8_t* s);
QB_API qbVar       qbCString(utf8_t* s);
QB_API qbVar       qbBytes(const char* bytes, size_t size);

QB_API qbRef       qb_ref_at(qbVar v, qbVar key);
QB_API qbVar       qb_var_at(qbVar v, qbVar key);

QB_API qbVar*      qb_array_at(qbVar array, int32_t i);
QB_API qbVar*      qb_array_append(qbVar array, qbVar v);
QB_API void        qb_array_insert(qbVar array, qbVar v, int32_t i);
QB_API void        qb_array_erase(qbVar array, int32_t i);
QB_API void        qb_array_swap(qbVar array, int32_t i, int32_t j);
QB_API void        qb_array_resize(qbVar array, size_t size);
QB_API size_t      qb_array_count(qbVar array);
QB_API qbTag       qb_array_type(qbVar array);
QB_API qbVar*      qb_array_raw(qbVar array);
QB_API bool        qb_array_iterate(qbVar array, bool(*it)(qbVar* k, qbVar state), qbVar state);

QB_API qbVar*      qb_map_at(qbVar map, qbVar k);
QB_API qbVar*      qb_map_insert(qbVar map, qbVar k, qbVar v);
QB_API void        qb_map_erase(qbVar map, qbVar k);
QB_API void        qb_map_swap(qbVar map, qbVar a, qbVar b);
QB_API size_t      qb_map_count(qbVar map);
QB_API bool        qb_map_has(qbVar map, qbVar k);
QB_API qbTag       qb_map_keytype(qbVar map);
QB_API qbTag       qb_map_valtype(qbVar map);
QB_API bool        qb_map_iterate(qbVar map, bool(*it)(qbVar k, qbVar* v, qbVar state), qbVar state);

QB_API size_t      qb_var_pack(qbVar v, qbBuffer_* buf, ptrdiff_t* pos);
QB_API size_t      qb_var_unpack(qbVar* v, const qbBuffer_* buf, ptrdiff_t* pos);
QB_API void        qb_var_copy(const qbVar* from, qbVar* to);
QB_API void        qb_var_move(qbVar* from, qbVar* to);
QB_API void        qb_var_destroy(qbVar* v);
QB_API qbVar       qb_var_tostring(qbVar v);

// Represents a non-value.
QB_API extern const qbVar qbNil;

// Represents a qbVar that is not yet set. Used to represent a value that will
// be set in the future.
QB_API extern const qbVar qbFuture;

///////////////////////////////////////////////////////////
//////////////////////  Flow Control  /////////////////////
///////////////////////////////////////////////////////////

typedef enum {
  QB_FEATURE_ALL,
  QB_FEATURE_LOGGER = 0x0001,
  QB_FEATURE_INPUT = 0x0002,
  QB_FEATURE_GRAPHICS = 0x0004,
  QB_FEATURE_AUDIO = 0x0008,
  QB_FEATURE_GAME_LOOP = 0x0010,
} qbFeature;

// Holds the game engine state
typedef struct qbUniverse {
  void* self;

  qbFeature enabled;
  uint64_t frame;
} qbUniverse;

typedef struct qbScriptAttr_ {
  char** directory;
  const char* entrypoint;
} qbScriptAttr_;

typedef struct qbSchedulerAttr_ {
  size_t max_async_coros;
  size_t max_async_tasks;
} qbSchedulerAttr_;

typedef struct {
  const char* title;
  uint32_t width;
  uint32_t height;

  qbFeature enabled;

  struct qbRendererAttr_* renderer_args;
  struct qbAudioAttr_* audio_args;
  struct qbScriptAttr_* script_args;
  struct qbSchedulerAttr_* scheduler_args;
} qbUniverseAttr_, *qbUniverseAttr;

QB_API qbResult qb_init(qbUniverse* universe, qbUniverseAttr attr);
QB_API qbResult qb_start();
QB_API qbResult qb_stop();
QB_API bool qb_running();

typedef struct qbLoopCallbacks_ {
  void(*on_update)(uint64_t frame, qbVar);
  void(*on_fixedupdate)(uint64_t frame, qbVar);
  void(*on_prerender)(struct qbRenderEvent_*, qbVar);
  void(*on_postrender)(struct qbRenderEvent_*, qbVar);
  void(*on_resize)(uint32_t width, uint32_t height, qbVar);
} qbLoopCallbacks_, *qbLoopCallbacks;

typedef struct {
  qbVar update;
  qbVar fixed_update;
  qbVar prerender;
  qbVar postrender;
  qbVar resize;
} qbLoopArgs_, *qbLoopArgs;

QB_API qbResult qb_loop(qbLoopCallbacks callbacks, qbLoopArgs args);

typedef struct qbTiming_ {
  uint64_t frame;
  double udpate_fps;
  double render_fps;
  double total_fps;

  uint64_t total_elapsed_ns;
  uint64_t render_elapsed_ns;

  double* update_elapsed_samples;
  size_t update_elapsed_samples_count;

} qbTiming_, *qbTiming;
QB_API qbResult qb_timing(qbUniverse universe, qbTiming timing);

// Unimplemented.
QB_API qbResult qb_save(const char* file);

// Unimplemented.
QB_API qbResult qb_load(const char* file);

// ======== qbProgram ========
// A program is a structure that holds all encapsulated state. This includes
// collections, systems, events, and subscriptions. Programs can only talk to
// each other through messages. Programs can access each other's collections
// only after it has been shared.
typedef struct qbProgram {
  const qbId id;
  const char* name;
  const void* self;
} qbProgram;

// Creates a program with the specified name. Copies the name into new memory.
// This new program is run on a separate thread.
//
// Before each loop the following occurs:
// 1) onready is called on the main program
// 2) onready is called on all other non-detached programs in a
//    non-deterministic order
// 3) all non-detached programs run
// 4) the main program runs and blocks until completion
// 5) all non-detached programs are joined and block execution until completion
QB_API qbId qb_program_create(const char* name);

// Sets the onready function in the program.
QB_API void qb_program_onready(qbId program,
                               void(*onready)(qbProgram* program, qbVar), qbVar state);

// Runs a particular program flushing its events then running all of its systems.
QB_API qbResult qb_program_run(qbId program);

// Detaches a program from the main game loop. This starts an asynchronous
// thread.
QB_API qbResult qb_program_detach(qbId program);

// Joins a program with the main game loop.
QB_API qbResult qb_program_join(qbId program);

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
typedef enum qbComponentType {
  // A piece of serializable memory.
  QB_COMPONENT_TYPE_RAW = 0,

  // A pointer to a piece of memory. Will be freed when instance is destroyed.
  // Pointer must not be allocated with new or new[] operators.
  QB_COMPONENT_TYPE_POINTER,

  // A struct only comprised of "qbEntity"s as its members. Will destroy all
  // entities inside of the struct.
  QB_COMPONENT_TYPE_COMPOSITE,

  // A struct that uses a schema to create a dynamic struct.
  QB_COMPONENT_TYPE_SCHEMA,
} qbComponentType;

// ======== qbComponentAttr ========
// Creates a new qbComponentAttr object for qbComponent creation.
QB_API qbResult      qb_componentattr_create(qbComponentAttr* attr);

// Destroys the specified attribute.
QB_API qbResult      qb_componentattr_destroy(qbComponentAttr* attr);

// Sets the allocated size for the component.
QB_API qbResult      qb_componentattr_setdatasize(qbComponentAttr attr,
                                                  size_t size);

// Sets the component type which tells the engine how to handle the component.
QB_API qbResult      qb_componentattr_settype(qbComponentAttr attr,
                                              qbComponentType type);

// Sets the schema of the component. This is used to dynamically create
// components with custom fields. This is used by the Lua system to create
// components from a user-supplied script.
QB_API qbResult      qb_componentattr_setschema(qbComponentAttr attr,
                                                qbSchema schema);

// Sets the component to be shared across programs with a reader/writer lock.
// By default a component is "shared", i.e. is_shared=true.
QB_API qbResult      qb_componentattr_setshared(qbComponentAttr attr, bool is_shared);

// Unimplemented.
QB_API qbResult      qb_componentattr_onpack(qbComponentAttr attr,
                                             void(*fn)(qbComponent component, qbEntity entity,
                                                       const void* read, qbBuffer_* buf, ptrdiff_t* pos));

// Unimplemented.
QB_API qbResult      qb_componentattr_onunpack(qbComponentAttr attr,
                                               void(*fn)(qbComponent component, qbEntity entity,
                                                         void* write, const qbBuffer_* read, ptrdiff_t* pos));

// Sets the data type for the component.
// Same as qb_componentattr_setdatasize(attr, sizeof(type)).
#define qb_componentattr_setdatatype(attr, type) \
    qb_componentattr_setdatasize(attr, sizeof(type))

// ======== qbComponent ========

// Creates a new qbComponent with the specified attributes.
QB_API qbResult      qb_component_create(qbComponent* component,
                                         const char* name,
                                         qbComponentAttr attr);

// Destroys the specified qbComponent.
QB_API qbResult      qb_component_destroy(qbComponent* component);

// Returns the number of specified components.
QB_API size_t        qb_component_count(qbComponent component);

// Returns the component from the given name or -1 if not found.
QB_API qbComponent   qb_component_find(const char* name, qbSchema* schema);

// Returns the schema of the component.
QB_API qbSchema      qb_component_schema(qbComponent component);

// Unimplemented.
QB_API size_t        qb_component_size(qbComponent component);

// Unimplemented.
QB_API size_t        qb_component_pack(qbComponent component, qbEntity entity, const void* read,
                                       qbBuffer_* write, int* pos);

// Unimplemented.
QB_API size_t        qb_component_unpack(qbComponent component, qbEntity entity, void* data,
                                         const qbBuffer_* read, int* pos);

QB_API qbResult      qb_component_oncreate(qbComponent component, qbSystem system);
QB_API qbResult      qb_component_ondestroy(qbComponent component, qbSystem system);


///////////////////////////////////////////////////////////
////////////////////////  Instances  //////////////////////
///////////////////////////////////////////////////////////

// ======== qbInstance ========
// A qbInstance is a per-program and per-thread (not thread-safe) entity
// combined with its component references.

// Triggers fn when a instance of the given component is created. If created
// when an entity is created, then it is triggered after all components have
// been instantiated.
QB_API qbResult      qb_instance_oncreate(qbComponent component,
                                          void(*fn)(qbInstance instance, qbVar state),
                                          qbVar state);

// Triggers fn when a instance of the given component is destroyed. Is
// triggered before before memory is freed.
QB_API qbResult      qb_instance_ondestroy(qbComponent component,
                                           void(*fn)(qbInstance instance, qbVar state),
                                           qbVar state);

// Gets a read-only view of a component instance for the given entity.
QB_API qbResult      qb_instance_find(qbComponent component,
                                      qbEntity entity,
                                      void* pbuffer);

// Returns the entity that contains this component instance.
QB_API qbEntity      qb_instance_entity(qbInstance instance);

// Fills pbuffer with component instance data. If the parent component is
// shared, this locks the reader lock. If the instance's component has a
// schema, then the pointer to the schema's data is returned.
QB_API qbResult      qb_instance_const(qbInstance instance,
                                          void* pbuffer);

// Fills pbuffer with component instance data. If the parent component is
// shared, this locks the writer lock. If the instance's component has a
// schema, then the pointer to the schema's data is returned.
QB_API qbResult     qb_instance_mutable(qbInstance instance,
                                           void* pbuffer);

// Fills pbuffer with component instance data. The memory is mutable.
QB_API qbResult     qb_instance_component(qbInstance instance,
                                             qbComponent component,
                                             void* pbuffer);

// Returns true if the entity containing the instance also contains a specified
// component.
QB_API bool        qb_instance_hascomponent(qbInstance instance,
                                            qbComponent component);

// Returns a qbRef to the memory pointed at by key in the struct. Assumes that
// the instance has an schema.
QB_API qbRef       qb_instance_at(qbInstance instance, const char* key);

///////////////////////////////////////////////////////////
////////////////////////  Entities  ///////////////////////
///////////////////////////////////////////////////////////

// ======== qbEntityAttr ========
// Creates a new qbEntityAttr object for entity creation.
QB_API qbResult      qb_entityattr_create(qbEntityAttr* attr);

// Destroys the specified attribute.
QB_API qbResult      qb_entityattr_destroy(qbEntityAttr* attr);

// Adds a component with instance data to be copied into the entity.
// This only copies the instance_data pointer and does not allocate new memory.
QB_API qbResult      qb_entityattr_addcomponent(qbEntityAttr attr,
                                                qbComponent component,
                                                void* instance_data);

// Returns the size in bytes of the qbEntityAttr_ struct.
QB_API size_t        qb_entityattr_size();

// Initializes a qbEntityAttr. This is only used with
// qb_entityattr_create__unsafe.
QB_API qbResult      qb_entityattr_init(qbEntityAttr* attr);

#define qb_entityattr_create__unsafe(attr) \
do { (*attr) = (qbEntityAttr)alloca(qb_entityattr_size()); \
     qb_entityattr_init(attr);\
} while(0)

// ======== qbEntity ========
// A qbEntity is an identifier to a game object. qbComponents can be added to
// the entity.

QB_API extern const qbEntity qbInvalidEntity;

// Creates a new qbEntity with the specified attributes.
// This is only thread-safe if all of the attached components are "shared"
// components, created with the `qb_componentattr_setshared` method.
QB_API qbResult      qb_entity_create(qbEntity* entity,
                                      qbEntityAttr attr);

// Creates a new empty qbEntity.
QB_API qbEntity      qb_entity_empty();

// Destroys the specified entity and all of its components. This destroys the
// entity at the end of the frame ensuring that references to entities are
// always valid during a frame.
// This is only thread-safe if all of the attached components are "shared"
// components, created with the `qb_componentattr_setshared` method.
QB_API qbResult      qb_entity_destroy(qbEntity entity);

// Adds a component with instance data to copied to the entity.
// This allocates a new instance copies the instance_data to the newly
// allocated memory. This calls the instance's OnCreate function immediately.
// This is only thread-safe if the component is a "shared" component, created
// with the `qb_componentattr_setshared` method.
QB_API qbResult      qb_entity_addcomponent(qbEntity entity,
                                            qbComponent component,
                                            void* instance_data);

// Removes the specified component from the entity. Does not remove the
// component until after the current frame has completed. This calls the
// instance's OnDestroy function after the current frame has completed.
// This is only thread-safe if the component is a "shared" component, created
// with the `qb_componentattr_setshared` method.
QB_API qbResult      qb_entity_removecomponent(qbEntity entity,
                                               qbComponent component);

// Returns true if the specified entity contains an instance for the component.
// This is only thread-safe if the component is a "shared" component, created
// with the `qb_componentattr_setshared` method.
QB_API bool          qb_entity_hascomponent(qbEntity entity,
                                            qbComponent component);

// Returns the specified component.
// This is only thread-safe if the component is a "shared" component, created
// with the `qb_componentattr_setshared` method.
QB_API void*         qb_entity_getcomponent(qbEntity entity,
                                            qbComponent component);

///////////////////////////////////////////////////////////
////////////////////////  Systems  ////////////////////////
///////////////////////////////////////////////////////////

// ======== qbFrame ========
// a qbFrame is a struct that is filled in during execution time. If the system
// was triggered by an event, the "event" member will point to its message. If
// the system has user state, defined with "setuserstate" this will be filled
// in.
typedef struct qbFrame {
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
QB_API qbResult      qb_barrier_create(qbBarrier* barrier);

// Destroys a barrier.
QB_API qbResult      qb_barrier_destroy(qbBarrier* barrier);

// ======== qbSystemAttr ========
// Creates a new qbSystemAttr object for system creation.
QB_API qbResult      qb_systemattr_create(qbSystemAttr* attr);

// Destroys the specified attribute.
QB_API qbResult      qb_systemattr_destroy(qbSystemAttr* attr);

// Adds a read-only component to the be read when the system is run.
QB_API qbResult      qb_systemattr_addconst(qbSystemAttr attr,
                                            qbComponent component);

// Adds a mutable component to the be read when the system is run.
QB_API qbResult      qb_systemattr_addmutable(qbSystemAttr attr,
                                              qbComponent component);

// ======== qbComponentJoin ========
typedef enum qbComponentJoin {
  QB_JOIN_INNER = 0,
  QB_JOIN_LEFT,
  QB_JOIN_CROSS,
} qbComponentJoin;

// Instructs the execution of the system to join together multiple components.
QB_API qbResult      qb_systemattr_setjoin(qbSystemAttr attr,
                                           qbComponentJoin join);

// Sets the program where the system will be run. By default, the system is run
// on the same thread as "qb_loop()".
QB_API qbResult      qb_systemattr_setprogram(qbSystemAttr attr,
                                              qbId program);

// Sets the transform to run during execution. The specified transform will be
// run on every component instance that was added with "addconst" and
// "addmutable".
typedef void(*qbTransformFn)(qbInstance* instances, qbFrame* frame);
QB_API qbResult      qb_systemattr_setfunction(qbSystemAttr attr,
                                               qbTransformFn transform);

// Sets the callback to run after the system finishes executing its transform
// over all of its components.
typedef void(*qbCallbackFn)(qbFrame* frame);
QB_API qbResult      qb_systemattr_setcallback(qbSystemAttr attr,
                                               qbCallbackFn callback);

// Allows the system to execute if the specified condition returns true.
typedef bool(*qbConditionFn)(qbFrame* frame);
QB_API qbResult      qb_systemattr_setcondition(qbSystemAttr attr,
                                                qbConditionFn condition);

// ======== qbTrigger ========
typedef enum qbTrigger {
  QB_TRIGGER_LOOP = 0,
  QB_TRIGGER_EVENT
} qbTrigger;
// Sets the trigger for the system. Systems by default are triggered by the
// main execution loop with "qb_loop()". To detach a system to only be run
// for an event, use the QB_TRIGGER_EVENT value.
QB_API qbResult      qb_systemattr_settrigger(qbSystemAttr attr,
                                              qbTrigger trigger);

// ======== Priorities ========
const int16_t QB_MAX_PRIORITY = (int16_t)0x7FFF;
const int16_t QB_MIN_PRIORITY = (int16_t)0x8001;
// Sets the priority for the system. Systems with higher priority values will
// be run before systems with lower priorities.
QB_API qbResult      qb_systemattr_setpriority(qbSystemAttr attr,
                                               int16_t priority);

// Adds a barrier to the system to enforce ordering across programs.
QB_API qbResult      qb_systemattr_addbarrier(qbSystemAttr attr,
                                              qbBarrier barrier);

// Sets a pointer to be passed in with every execution of the system.
QB_API qbResult      qb_systemattr_setuserstate(qbSystemAttr attr,
                                                void* state);

// ======== qbSystem ========
// A qbSystem is the atomic unit of synchronous execution. Systems are run when
// either: qb_loop() is called, a program is detached and runs continuously, or
// an event is triggered. Once a system is executed, it will iterate through
// all of its specified components and execute its transform over every
// instance.
// Creates a new qbSystem with the specified attributes.
QB_API qbResult      qb_system_create(qbSystem* system,
                                      qbSystemAttr attr);

// Destroys the specified system.
QB_API qbResult      qb_system_destroy(qbSystem* system);

// Enables the specified system and resume all execution.
QB_API qbResult      qb_system_enable(qbSystem system);

// Disables the specified system and stop all execution.
QB_API qbResult      qb_system_disable(qbSystem system);

// Runs the given system. Not thread-safe when run concurrently with qb_loop().
// Unimplemented.
QB_API qbResult      qb_system_run(qbSystem system);


///////////////////////////////////////////////////////////
//////////////////  Events and Messaging  /////////////////
///////////////////////////////////////////////////////////

// ======== qbEventAttr ========
// Creates a new qbEventAttr object for event creation.
QB_API qbResult      qb_eventattr_create(qbEventAttr* attr);

// Destroys the specified attributes.
QB_API qbResult      qb_eventattr_destroy(qbEventAttr* attr);

// Sets which program to associate the event with. The event will only trigger
// inside the specified program.
QB_API qbResult      qb_eventattr_setprogram(qbEventAttr attr,
                                             qbId program);

// Sets the size of each message to be allocated to send.
QB_API qbResult      qb_eventattr_setmessagesize(qbEventAttr attr, size_t size);
#define qb_eventattr_setmessagetype(attr, type) \
    qb_eventattr_setmessagesize(attr, sizeof(type))

// ======== qbEvent ========
// A qbEvent is a way of passing messages between systems in a single program.
// Sending messages is not thread-safe.
// Creates a new qbEvent with the specified attributes.
QB_API qbResult      qb_event_create(qbEvent* event,
                                     qbEventAttr attr);

// Destroys the specified event.
QB_API qbResult      qb_event_destroy(qbEvent* event);

// Flushes all events from the specified program.
QB_API qbResult      qb_event_flushall(qbProgram program);

// Subscribes the specified system to the event. This system will execute any
// time the event is triggered. The system must have a trigger of
// QB_TRIGGER_EVENT.
QB_API qbResult      qb_event_subscribe(qbEvent event,
                                        qbSystem system);

// Unsubscribes the specified system from the event.
QB_API qbResult      qb_event_unsubscribe(qbEvent event,
                                          qbSystem system);

// Sends a messages on the event. This triggers all subscribed systems before
// the next frame is run.
QB_API qbResult      qb_event_send(qbEvent event,
                                   void* message);

// Sends a messages on the event. This immediately triggers all subscribed
// systems.
QB_API qbResult      qb_event_sendsync(qbEvent event,
                                       void* message);


///////////////////////////////////////////////////////////
/////////////////////////  Scenes  ////////////////////////
///////////////////////////////////////////////////////////

// Creates a scene with the given name. The scene is created in an "unset"
// state. To start working on a scene, use the qb_scene_set method.
QB_API qbResult      qb_scene_create(qbScene* scene,
                                     const char* name);

// Unimplemented.
QB_API qbResult      qb_scene_save(qbScene* scene,
                                   const char* file);

// Unimplemented.
QB_API qbResult      qb_scene_load(qbScene* scene,
                                   const char* name,
                                   const char* file);

// Destroys the given scene. Order of operations:
// 1. Calls the ondestroy event on the given scene
// 2. Destroys all alive entities and calls the ondestroy event
// 3. Activates the Global Scene while and calls the onactivate event
// 4. Sets the "working scene" to be the Global Scene
QB_API qbResult      qb_scene_destroy(qbScene* scene);

// Returns the Global Scene singleton. This scene is created at the start of
// the engine. This is useful if there are resources or entities that are
// needed throughout the lifetime of the game.
QB_API qbScene       qb_scene_global();

// Sets the given scene to be the "working scene". Once a scene is set, any
// created entities will be scoped to the lifetime of the given scene.
// Usage:
// qbScene main_menu;
// qb_scene_create(&main_menu, "Main Menu");
// qb_scene_set(main_menu);
// ... create entities ...
// qb_scene_activate(main_menu);
QB_API qbResult      qb_scene_set(qbScene scene);

// Resets the "working scene" to the currently active scene.
QB_API qbResult      qb_scene_reset();

#define qb_scene_with(scene, expr) do { qb_scene_set(scene); expr; qb_scene_reset(); } while(0)

// Returns the name of the given scene.
QB_API const char*   qb_scene_name(qbScene scene);

// Activates the given scene. Order of operations:
// 1. Deactivates current active scene and calls the ondeactivate event
// 2. Activates the given scene
// 3. Sets the "working scene" to the given scene
// 4. Calls the onactivate event with the given scene
QB_API qbResult      qb_scene_activate(qbScene scene);

// Attaches the given key-value pair to the scene. These key-value pairs are
// then given whenever the scene is activated/deactivated or destroyed.
QB_API qbResult      qb_scene_attach(qbScene scene, const char* key, void* value);

// Triggers fn when scene is destroyed. Can be called multiple times to add
// more than one handler.
QB_API qbResult      qb_scene_ondestroy(qbScene scene,
                                        void(*fn)(qbScene scene,
                                                  size_t count,
                                                  const char* keys[],
                                                  void* values[]));

// Triggers fn when scene is activated. Can be called multiple times to add
// more than one handler.
QB_API qbResult      qb_scene_onactivate(qbScene scene,
                                         void(*fn)(qbScene scene,
                                                   size_t count,
                                                   const char* keys[],
                                                   void* values[]));

// Triggers fn when scene is deactivated. Can be called multiple times to add
// more than one handler.
QB_API qbResult      qb_scene_ondeactivate(qbScene scene,
                                           void(*fn)(qbScene scene,
                                                     size_t count,
                                                     const char* keys[],
                                                     void* values[]));


///////////////////////////////////////////////////////////
///////////////////////  Coroutines  //////////////////////
///////////////////////////////////////////////////////////

// Creates and returns a new coroutine only valid on the current thread.
// Cannot be passed between threads.
QB_API qbCoro      qb_coro_create(qbVar(*entry)(qbVar var));

// Copies a given coroutine only valid on the current thread. Does not copy the
// coroutine state. Currently, only copies the entry function.
// Cannot be passed between threads.
QB_API qbCoro      qb_coro_copy(qbCoro coro);

// A coroutine is safe to destroy only it is finished running. This can be
// queried with qb_coro_peek or qb_coro_done. A coroutine can be waited upon by
// using qb_coro_await.
QB_API qbResult    qb_coro_destroy(qbCoro* coro);

// Immediately runs the given coroutine on the same thread as the caller.
// WARNING: A coroutine has its own stack, do not pass in pointers to stack
// variables. They will be invalid pointers.
QB_API qbVar       qb_coro_call(qbCoro coro, qbVar var);

// Creates a coroutine and schedules the given function to be run on the main
// thread. All coroutines are then run serially after event dispatch and
// systems are run. All coroutines are run as cooperative threads. In order to
// run the next coroutine, the given entry function must call qb_coro_yield().
// WARNING: A coroutine has its own stack, do not pass in pointers to stack
// variables. They will be invalid pointers.
QB_API qbCoro      qb_coro_sync(qbVar(*entry)(qbVar), qbVar var);

// Creates a coroutine and schedules the given function to be run on a
// background thread. Thread-safe.
// WARNING: A coroutine has its own stack, do not pass in pointers to stack
// variables. They will be invalid pointers.
// WARNING: Any async coros still running when qb_stop() is called will block
// shutting down. Use `qb_running()` to check state of game engine and return
// early.
QB_API qbCoro      qb_coro_async(qbVar(*entry)(qbVar), qbVar var);

// Yields control with the given var back to the current coroutine's caller.
// WARNING: A coroutine has its own stack, do not pass in pointers to stack
// variables. They will be invalid pointers.
QB_API qbVar       qb_coro_yield(qbVar var);

// Yields "qbFuture" until at least the given seconds have elapsed.
QB_API void        qb_coro_wait(double seconds);

// Yields "qbFuture" until the given frames have elapsed.
QB_API void        qb_coro_waitframes(uint32_t frames);

// Yields "qbFuture" until coro is done running. 
QB_API qbVar       qb_coro_await(qbCoro coro);

// Peeks at the return value of the scheduled coro. Returns "qbFuture" if the
// scheduled coro is running.
QB_API qbVar      qb_coro_peek(qbCoro coro);

// Returns true if Coroutine is finished running.
QB_API bool       qb_coro_done(qbCoro coro);


#endif  // #ifndef CUBEZ__H
