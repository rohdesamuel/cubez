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

#define QB_MAJOR_VERSION 1
#define QB_MINOR_VERSION 1
#define QB_PATCH_VERSION 0

#include <cubez/common.h>
#include <cubez/log.h>
#include <cubez/buffer.h>
#include <cubez/var.h>

///////////////////////////////////////////////////////////
//////////////////////  Flow Control  /////////////////////
///////////////////////////////////////////////////////////

#define QB_FEATURE_ALL 0x0000
#define QB_FEATURE_INPUT 0x0001
#define QB_FEATURE_GRAPHICS 0x0002
#define QB_FEATURE_AUDIO 0x0004
#define QB_FEATURE_GAME_LOOP 0x0008
typedef uint32_t qbFeature;

// Macro to abstract away Win32-specific entrypoint.
// Usage:
// int qb_main(int argc, char* argv[]) { ... }
#if !defined(__BUILDING_DLL__) && !defined(qb_main)
#if defined(_DEBUG) || defined(__COPMILE_AS_LINUX__) 
#define qb_main(argc, argv) \
__qb_main(argc, argv); \
int main(int _argc, char* _argv[]) { \
  return __qb_main(_argc, _argv); \
} int __qb_main(argc, argv)
#elif defined(__COMPILE_AS_WINDOWS__)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#define qb_main(argc, argv) \
__qb_main(argc, argv); \
int WINAPI wWinMain( \
  _In_ HINSTANCE hInstance, \
  _In_opt_ HINSTANCE hPrevInstance, \
  _In_ LPWSTR lpCmdLine, \
  _In_ int nShowCmd) { \
  return __qb_main(__argc, __argv); \
}; int __qb_main(argc, argv)
#endif
#endif

// Holds the game engine state
typedef struct qbUniverse {
  void* self;

  int argc;
  char** argv;
  wchar_t** wargv;

  qbFeature enabled;
  uint64_t frame;
} qbUniverse;

typedef struct qbScriptAttr_ { 
  // First script to run.
  // Default "main.lua".
  const char* entrypoint;
} qbScriptAttr_;

typedef struct qbSchedulerAttr_ {
  // Default is 16.
  size_t max_async_coros;

  // Default is number of CPU cores.
  size_t max_async_tasks;

  // Default is 1024.
  size_t max_async_tasks_queue_size;
} qbSchedulerAttr_;

typedef struct qbResourceAttr_ {
  // Relative path from binary to load resources from.
  // Default is the directory where the executable runs from.
  const utf8_t* resources;

  // All the following paths default to load directly from the "resources" directory.
  // If specified, are relative from the "resources" directory.
  const utf8_t* scripts;
  const utf8_t* fonts;
  const utf8_t* sounds;
  const utf8_t* images;
  const utf8_t* meshes;  
} qbResourceAttr_, *qbResourceAttr;

typedef struct qbLoggingAttr_ {
  // Path to the directory to write game logs.
  // Default is "logs".
  const utf8_t* logs;
  
  // The maximum size of the log file. Once the limit is passed, no more logs
  // will be written. If max_log_size is set to 0, then the default size limit
  // is 1GB.
  size_t max_log_size;
} qbLoggingAttr_, *qbLoggingAttr;

typedef struct {
  const utf8_t* title;
  uint32_t width;
  uint32_t height;

  qbFeature enabled;

  struct qbRendererAttr_* renderer_args;
  struct qbAudioAttr_* audio_args;
  struct qbScriptAttr_* script_args;
  struct qbSchedulerAttr_* scheduler_args;
  struct qbResourceAttr_* resource_args;
  struct qbLoggingAttr_* logging_args;
} qbUniverseAttr_, *qbUniverseAttr;

QB_API qbResult qb_init(qbUniverse* universe, qbUniverseAttr attr);
QB_API qbResult qb_start();
QB_API qbResult qb_stop();
QB_API void qb_pause();
QB_API void qb_resume();

QB_API qbBool qb_running();
QB_API const qbResourceAttr_* qb_resources();
QB_API const utf8_t* qb_dir();

typedef struct qbLoopCallbacks_ {
  void(*on_update)(uint64_t frame, qbVar);
  void(*on_fixedupdate)(uint64_t frame, qbVar);
  void(*on_render)(struct qbRenderEvent_*, qbVar);
  void(*on_postrender)(struct qbRenderEvent_*, qbVar);
  void(*on_resize)(uint32_t width, uint32_t height, qbVar);
} qbLoopCallbacks_, *qbLoopCallbacks;

typedef struct {
  qbVar update;
  qbVar fixed_update;
  qbVar render;
  qbVar postrender;
  qbVar resize;
} qbLoopArgs_, *qbLoopArgs;

QB_API qbResult qb_loop(qbLoopCallbacks callbacks, qbLoopArgs args);

typedef struct qbTiming_ {
  uint64_t frame;
  int64_t frametime_ns;
  double udpate_fps;
  double render_fps;
  double total_fps;

  uint64_t total_elapsed_ns;
  uint64_t render_elapsed_ns;

  double* update_elapsed_samples;
  size_t update_elapsed_samples_count;

} qbTiming_, *qbTiming;
QB_API qbResult qb_timing(qbUniverse universe, qbTiming timing);

QB_API uint64_t qb_framenum();

QB_API int64_t  qb_frametime_ns();

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

// Returns the lua state of the main program.
QB_API struct lua_State* qb_luastate();

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
typedef void(*qbEventFn)(void*, qbVar);

///////////////////////////////////////////////////////////
///////////////////////  Components  //////////////////////
///////////////////////////////////////////////////////////

// ======== qbComponentType ========
typedef enum qbComponentType {
  // A piece of serializable memory. Freeing of any allocations must be handled
  // by the user.
  QB_COMPONENT_TYPE_RAW = 0,

  // A pointer to a piece of memory. Will be freed when instance is destroyed.
  // Pointer must not be allocated with new or new[] operators when used with
  // C++.
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
QB_API qbResult      qb_componentattr_setshared(qbComponentAttr attr, qbBool is_shared);

// Sets the component's packing function to allow to be serialized.
// Method should update `pos` to the byte past the last byte written and
// return the number of bytes written.
QB_API qbResult      qb_componentattr_onpack(qbComponentAttr attr,
                                             size_t(*fn)(qbComponent component, const void* read,
                                                         qbBuffer_* write, ptrdiff_t* pos));

// Sets the component's packing function to allow to be deserialized.
// Method should update `pos` to the byte past the last byte written and
// return the number of bytes written.
QB_API qbResult      qb_componentattr_onunpack(qbComponentAttr attr,
                                               size_t(*fn)(qbComponent component, const void* read,
                                                           qbBuffer_* write, ptrdiff_t* pos));

// Sets the data type for the component.
// Same as qb_componentattr_setdatasize(attr, sizeof(type)).
#define qb_componentattr_setdatatype(attr, type) \
    qb_componentattr_setdatasize(attr, sizeof(type))

// ======== qbComponent ========

QB_API extern const qbComponent qbInvalidComponent;

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

// Serializes the given `read` buffer containing `component` data to the
// `write` buffer. Updates `pos` to the byte past the last byte written.
QB_API size_t        qb_component_pack(qbComponent component, const qbBuffer_* read,
                                       qbBuffer_* write, ptrdiff_t* pos);

// Deserializes the given `read` buffer containing serialized` component` data
// to the `write` buffer. Updates `pos` to the byte past the last byte written.
QB_API size_t        qb_component_unpack(qbComponent component, const qbBuffer_* read,
                                         qbBuffer_* write, ptrdiff_t* pos);

QB_API qbResult      qb_component_oncreate(qbComponent component, qbEventFn fn, qbVar arg);
QB_API qbResult      qb_component_ondestroy(qbComponent component, qbEventFn fn, qbVar arg);


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

// Fills pbuffer with component instance data. The memory is mutable.
QB_API qbResult     qb_instance_component(qbInstance instance,
                                          qbComponent component,
                                          void* pbuffer);

// Returns true if the entity containing the instance also contains a specified
// component.
QB_API qbBool       qb_instance_hascomponent(qbInstance instance,
                                             qbComponent component);

// Fills the given variadic arguments with the corresponding component.
// This is only supported to be called from qbSystems.
#define qb_instance_get(QB_INSTANCE, ...) qb_instance_get_(QB_INSTANCE, __VA_ARGS__, 0xCD)
QB_API void        qb_instance_get_(qbInstance instance, ...);
QB_API void        qb_instance_geti(qbInstance instance, size_t index, void* pbuf);
QB_API void        qb_instance_getn(qbInstance instance, size_t count, void* pbufs[]);

QB_API qbVar       qb_instance_struct(qbInstance instance);

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

typedef struct qbComponentData_ {
  qbComponent component;
  void* data;
} qbComponentData_;

// Creates a new qbEntity with the specified components and returns the created entity.
// A convience method for qb_entity_withlen.
// Assumes that the parameter is an array defined on the stack.
#define qb_entity_with(QB_COMPONENT_DATA_ARRAY) \
  qb_entity_withlen(sizeof(QB_COMPONENT_DATA_ARRAY) / sizeof((QB_COMPONENT_DATA_ARRAY)[0]), QB_COMPONENT_DATA_ARRAY)

// Creates a new qbEntity with the specified components and returns the created entity.
// This is only thread-safe if all of the attached components are "shared"
// components, created with the `qb_componentattr_setshared` method.
QB_API qbEntity      qb_entity_withlen(size_t count, const qbComponentData_ data[]);

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

// This allocates a new instance copies the instance_data to the newly
// allocated memory. This calls the instance's OnCreate function immediately.
// This is only thread-safe if the component is a "shared" component, created
// with the `qb_componentattr_setshared` method.
QB_API qbResult      qb_entity_addcomponents(qbEntity entity,
                                             size_t count,
                                             const qbComponentData_ data[]);

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
QB_API qbBool        qb_entity_hascomponent(qbEntity entity,
                                            qbComponent component);

// Returns the specified component.
// This is only thread-safe if the component is a "shared" component, created
// with the `qb_componentattr_setshared` method.
QB_API void*         qb_entity_getcomponent(qbEntity entity,
                                            qbComponent component);

// Returns the persistable id of the given entity.
// This is short-hand for `qb_entity_getcomponent(entity, qb_id())`.
QB_API uint64_t      qb_entity_uid(qbEntity entity);

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
  QB_JOIN_UNION,
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
typedef void(*qbTransformFn)(qbInstance instance, qbFrame* frame);
QB_API qbResult      qb_systemattr_setfunction(qbSystemAttr attr,
                                               qbTransformFn transform);

// Sets the callback to run after the system finishes executing its transform
// over all of its components.
typedef qbVar(*qbCallbackFn)(qbFrame* frame, qbVar arg);
QB_API qbResult      qb_systemattr_setcallback(qbSystemAttr attr,
                                               qbCallbackFn callback);

// Allows the system to execute if the specified condition returns true.
typedef qbBool(*qbConditionFn)(qbFrame* frame);
QB_API qbResult      qb_systemattr_setcondition(qbSystemAttr attr,
                                                qbConditionFn condition);

// ======== qbTrigger ========
typedef enum qbTrigger {
  QB_TRIGGER_LOOP = 0,
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
QB_API qbVar         qb_system_run(qbSystem system, qbVar arg);

QB_API qbResult      qb_system_foreach(size_t component_count, qbComponent components[],
                                       qbVar state, void(*fn)(qbInstance, qbVar));

typedef enum qbQueryResult {
  QB_QUERY_RESULT_DONE,
  QB_QUERY_RESULT_CONTINUE,
} qbQueryResult;

typedef struct qbQueryComponent_ {
  qbComponent component;
  qbBool is_mutable;
} qbQueryComponent_, *qbQueryComponent;

typedef struct qbQuery_ {
  qbQueryResult(*fn)(qbEntity entity, qbVar state, qbVar arg);

  size_t all_count;
  qbQueryComponent_* all;

  size_t any_count;
  qbQueryComponent_* any;

  size_t none_count;
  qbQueryComponent_* none;

  qbVar state;
} qbQuery_, *qbQuery;

QB_API qbResult      qb_query(qbQuery query, qbVar arg);

// ======== qbIterator ========
// A qbIterator is a simplified way to query for entities with a given set of
// components.
typedef struct qbIterator_ {
  char __state__[96];
} qbIterator_, *qbIterator;

// A maximum of 8 components can be queried in a single iterator.
#define QB_MAX_ITERATOR_COMPONENT_COUNT 8

// Creates an iterator querying for entities with all of the given components.
// The variadic argument is a list of qbComponents.
// An iterator is created in an invalid state and qb_iterator_next must be
// called first.
// 
// Example:
// qbIterator_ it = qb_component_iterator(position_component, velocity_component);
// while(qb_iterator_next(&it)) {
//   vec2* pos;
//   qb_iterator_get(&it, &pos);
// }
//
#define qb_component_iterate(QB_COMPONENT, ...) \
    qb_component_iterate_(QB_COMPONENT, __VA_ARGS__, qbInvalidComponent)

// This method should not be called directly. You should call
// qb_component_iterate instead.
QB_API qbIterator_  qb_component_iterate_(qbComponent component, ...);

// Creates an iterator querying for entities with all of the given components.
// 
// An iterator is created in an invalid state and qb_iterator_next must be
// called first.
// 
// Example:
// qbComponent components[] = { position_component, velocity_component };
// qbIterator_ it = qb_component_iteraten(
//     blocks_component, sizeof(components) / sizeof(components[0]), components);
// while(qb_iterator_next(&it)) {
//   Block* block;
//   vec2* pos;
//   qb_iterator_get(&it, &block, &pos);
// }
QB_API qbIterator_ qb_component_iteraten(qbComponent component, size_t count,
                                         qbComponent components[]);


// Increments the iterator and returns QB_TRUE if the iterator is at the end.
// An iterator is created in an invalid state and qb_iterator_next must be
// called first.
QB_API qbBool       qb_iterator_next(qbIterator it);

// Retrieve the queried components from the iterator.
// Example:
// vec3 *pos, *vel;
// qb_iterator_get(&it, &pos, &vel);
#define qb_iterator_get(QB_ITERATOR, ...) qb_iterator_get_(QB_ITERATOR, __VA_ARGS__, 0xCD)

// This method should not be called directly. You should call qb_iterator_get
// instead.
QB_API void         qb_iterator_get_(qbIterator it, ...);

// Retrieve the queried components from the iterator.
QB_API void         qb_iterator_getn(qbIterator it, size_t count, void* pbufs[]);

// Returns the entity the iterator is pointing to.
QB_API qbEntity     qb_iterator_entity(qbIterator it);

// Retrieve the component at the given index.
// The index is the index for the wanted component in the same position in the
// call to qb_component_iterate.
// Throws a debug assertion if the index is out-of-bounds.
QB_API void         qb_iterator_index(qbIterator it, size_t index, void* pbuf);

// Retrieve the given component from the iterator, returns QB_TRUE if successful.
QB_API qbBool       qb_iterator_component(qbIterator it, qbComponent component, void* pbuf);

// ======== qbEntityTable ========
// A qbEntityTable is a way to create a separate table of entities. This table
// is not globally queryable and the components can only be accessed by the set
// of qb_entitytable_* functions.
// qbEntityTables help in a few ways:
//   * One, creating a "private" set of components that cannot be accessed.
//   * Two, creating a set of entities that are all contiguous in memory.

typedef struct qbEntityTableAttr_* qbEntityTableAttr;
typedef struct qbEntityTable_* qbEntityTable;

#define QB_MAX_TABLES_COUNT 65536

QB_API qbResult qb_entitytableattr_create(qbEntityTableAttr* attr);
QB_API qbResult qb_entitytableattr_destroy(qbEntityTableAttr* attr);

// Adds the given component to the qbEntityTable. All ordered iterator
// accesses will have the same order as added components.
QB_API qbResult qb_entitytableattr_add(qbEntityTableAttr attr, qbComponent component);

QB_API qbResult qb_entitytableattr_addnullable(qbEntityTableAttr attr, qbComponent component);

// Creates a qbEntityTable. The maximum amount of tables at any given time is
// QB_MAX_TABLES_COUNT. The result will be QB_OK if successfully created.
QB_API qbResult qb_entitytable_create(qbEntityTable* table, qbEntityTableAttr attr);

// Destroys the given qbEntityTable.
// This also destroys all the entities within the table.
QB_API qbResult qb_entitytable_destroy(qbEntityTable* table);

// A special Component type that destroys the table and its entities when the
// original entity is destroyed.
// Component type: qbEntityTable_*
QB_API qbComponent qb_entitytable_component();

// Returns the number of entities in the table.
QB_API size_t qb_entitytable_count(qbEntityTable table);

// Creates an iterator querying for entities with all of the given components.
// If the component is in the table, then the table will be queried. Can mix
// and match components in and not in the table.
// 
// The variadic argument is a list of qbComponents.
// An iterator is created in an invalid state and qb_iterator_next must be
// called first.
// 
// Example:
// qbIterator_ it = qb_entitytable_iterate(table, position_component, velocity_component);
// while(qb_iterator_next(&it)) {
//   vec2* pos;
//   qb_iterator_get(&it, &pos);
// }
#define qb_entitytable_iterate(QB_ENTITYTABLE, ...) \
    qb_entitytable_iterate_(QB_ENTITYTABLE, __VA_ARGS__, qbInvalidComponent)

// This method should not be called directly. You should call
// qb_entitytable_iterate instead.
QB_API qbIterator_ qb_entitytable_iterate_(qbEntityTable table, ...);

// Creates an iterator querying for entities with all of the given components.
// If the component is in the table, then the table will be queried. Can mix
// and match components in and not in the table.
// 
// An iterator is created in an invalid state and qb_iterator_next must be
// called first.
// 
// Example:
// qbComponent components[] = { position_component, velocity_component };
// qbIterator_ it = qb_entitytable_iterate(
//     table, sizeof(components) / sizeof(components[0]), components);
// while(qb_iterator_next(&it)) {
//   vec2* pos;
//   qb_iterator_get(&it, &pos);
// }
QB_API qbIterator_ qb_entitytable_iteraten(qbEntityTable table, size_t count,
                                           qbComponent components[]);

// Inserts the given component data into the table. The components added
// should be in the same order as the order of
// qb_entitytableattr_addcomponent() calls.
#define qb_entitytable_insert(QB_ENTITYTABLE, ...) \
    qb_entitytable_insert_(QB_ENTITYTABLE, __VA_ARGS__, 0xCD)

// This method should not be called directly. You should call
// qb_entitytable_insert instead.
QB_API qbEntity qb_entitytable_insert_(qbEntityTable table, ...);

// Inserts the given component data into the table. The components added
// should be in the same order as the order of
// qb_entitytableattr_addcomponent() calls.
QB_API qbEntity qb_entitytable_insertn(qbEntityTable table, size_t count, void* pbufs[]);

QB_API qbEntity qb_entitytable_insertc(qbEntityTable table, size_t count, const qbComponentData_ data[]);

QB_API qbResult qb_entitytable_add(qbEntityTable table, qbEntity entity, size_t count, const qbComponentData_ data[]);

// If the amount is larger than the current capacity, this allocates the given
// number of entities.
QB_API void qb_entitytable_reserve(qbEntityTable table, size_t count);

// Removes and destroys the given entity and its associated components from the table.
QB_API void qb_entitytable_erase(qbEntityTable table, qbEntity entity);

// Removes and destroys all entity and its associated components from the table.
QB_API void qb_entitytable_clear(qbEntityTable table);

// Finds the given entity and component in the given table and places the pointer to the data in pbuf.
QB_API qbResult qb_entitytable_find(qbEntityTable table, qbEntity entity, qbComponent component, void* pbuf);


///////////////////////////////////////////////////////////
//////////////////  Events and Messaging  /////////////////
///////////////////////////////////////////////////////////

// ======== qbEventAttr ========
// Creates a new qbEventAttr object for event creation.
QB_API qbResult      qb_eventattr_create(qbEventAttr* attr);

// Destroys the specified attributes.
QB_API qbResult      qb_eventattr_destroy(qbEventAttr* attr);

// Sets the size of each message to be allocated to send.
QB_API qbResult      qb_eventattr_setmessagesize(qbEventAttr attr, size_t size);
#define qb_eventattr_setmessagetype(attr, type) \
    qb_eventattr_setmessagesize(attr, sizeof(type))

// ======== qbEvent ========
// A qbEvent is a way of asynchronously executing callbacks at the end of a
// frame. Events are defined per `qbScene`; events defined in one scene will
// not be run once the active scene changes. Events are run in the order in
// which they were added (FIFO).
// Sending messages to a given event is not thread-safe.
// Creates a new qbEvent with the specified attributes.
QB_API qbResult      qb_event_create(qbEvent* event,
                                     qbEventAttr attr);

// Destroys the specified event.
QB_API qbResult      qb_event_destroy(qbEvent* event);

// The callback function when an event is triggered.
typedef void(*qbEventFn)(void* event, qbVar arg);

// Subscribes the specified event handler to the event.
QB_API qbResult      qb_event_subscribe(qbEvent event,
                                        qbEventFn fn,
                                        qbVar arg);

// Unsubscribes the specified handler from the event.
QB_API qbResult      qb_event_unsubscribe(qbEvent event,
                                          qbEventFn fn);

// Sends a messages on the event. This triggers all subscribed event handlers
// before the next frame is run.
QB_API qbResult      qb_event_send(qbEvent event,
                                   void* message);

// Sends a messages on the event. This immediately triggers all event handlers.
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
                                   const utf8_t* file);

// Unimplemented.
QB_API qbResult      qb_scene_load(qbScene* scene,
                                   const char* name,
                                   const utf8_t* file);

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
QB_API qbBool     qb_coro_done(qbCoro coro);

// Component type: uint64_t
QB_API qbComponent qb_uid();

// Component type: qbEntity
QB_API qbComponent qb_entity();

#endif  // #ifndef CUBEZ__H
