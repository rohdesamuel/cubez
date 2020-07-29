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

#include <cubez/cubez.h>
#include <cubez/utils.h>
#include <cubez/render.h>
#include <cubez/audio.h>
#include "defs.h"
#include "private_universe.h"
#include "byte_vector.h"
#include "component.h"
#include "system_impl.h"
#include "utils_internal.h"
#include "coro_scheduler.h"
#include "input_internal.h"
#include "log_internal.h"
#include "render_internal.h"
#include "gui_internal.h"
#include "audio_internal.h"
#include "network_impl.h"
#include "lua_bindings.h"


#define AS_PRIVATE(expr) ((PrivateUniverse*)(universe_->self))->expr

const qbVar qbNone = { QB_TAG_PTR, 0 };
const qbVar qbFuture = { QB_TAG_NIL, 0 };

static qbUniverse* universe_ = nullptr;
qbTiming_ timing_info;
qbTimer fps_timer;
qbTimer update_timer;
qbTimer render_timer;
CoroScheduler* coro_scheduler;
Coro coro_main;

struct GameLoop {
  const double kClockResolution = 1e9;
  const double dt = 0.01;

  double t;
  double current_time;
  double start_time;
  double accumulator;
  bool is_running;
} game_loop;

qbResult qb_init(qbUniverse* u, qbUniverseAttr attr) {
  universe_ = u;
  u->enabled = attr->enabled;

  utils_initialize();
  coro_main = coro_initialize(u);
  
  // Initialize Lua before PrivateUniverse is constructed because the Lua VM
  // is initialized per-thread.
  {
    qbScriptAttr_ empty_args = {};
    lua_bindings_initialize(attr->script_args ? attr->script_args : &empty_args);
  }

  universe_->self = new PrivateUniverse();
  coro_scheduler = new CoroScheduler(4);

  qbResult ret = AS_PRIVATE(init());

  if (universe_->enabled == QB_FEATURE_ALL) {
    universe_->enabled = (qbFeature)0xFFFF;
  }
  if (universe_->enabled & QB_FEATURE_LOGGER) {
    log_initialize();
  }
  if (universe_->enabled & QB_FEATURE_INPUT) {
    input_initialize();
  }
  if (universe_->enabled & QB_FEATURE_GRAPHICS) {
    RenderSettings render_settings;
    render_settings.title = attr->title;
    render_settings.width = attr->width;
    render_settings.height = attr->height;
    render_settings.create_renderer = attr->create_renderer;
    render_settings.destroy_renderer = attr->destroy_renderer;

    qbRendererAttr_ empty_args = {};
    render_settings.opt_renderer_args = attr->renderer_args ? attr->renderer_args : &empty_args;
    render_initialize(&render_settings);
  }

  if (universe_->enabled & QB_FEATURE_AUDIO) {
    AudioSettings s = {};
    s.sample_frequency = attr->audio_args->sample_frequency;
    s.buffered_samples = attr->audio_args->buffered_samples;
    audio_initialize(s);
  }

  if (universe_->enabled & QB_FEATURE_GAME_LOOP) {
    qb_timer_create(&fps_timer, 50);
    qb_timer_create(&update_timer, 50);
    qb_timer_create(&render_timer, 50);
  }

  network_initialize();  

  return ret;
}

qbResult qb_start() {
  game_loop.t = 0.0;
  game_loop.current_time = qb_timer_query() * 0.000000001;
  game_loop.start_time = (double)qb_timer_query();
  game_loop.accumulator = 0.0;
  game_loop.is_running = true;
  return AS_PRIVATE(start());
}

qbResult qb_stop() {
  network_shutdown();
  audio_shutdown();
  render_shutdown();
  
  qbResult ret = AS_PRIVATE(stop());
  universe_ = nullptr;
  return ret;
}

qbResult loop(qbLoopCallbacks callbacks,
              qbLoopArgs args) {
  qb_timer_start(fps_timer);

  const static double extra_render_time = universe_->enabled & QB_FEATURE_GRAPHICS ? 0.0 : game_loop.dt;
  double new_time = qb_timer_query() * 0.000000001;
  double frame_time = new_time - game_loop.current_time;
  frame_time = std::min(0.25, frame_time);
  game_loop.current_time = new_time;

  if (game_loop.accumulator >= game_loop.dt) {
    qb_handle_input([]() {
      qb_stop();
      game_loop.is_running = false;
    });

    if (!game_loop.is_running) {
      return QB_OK;
    }
  }

  if (callbacks && callbacks->on_fixedupdate) {
    callbacks->on_fixedupdate(universe_->frame, args->fixed_update);
  }
  while (game_loop.accumulator >= game_loop.dt) {
    qb_timer_start(update_timer);
    if (callbacks && callbacks->on_update) {
      callbacks->on_update(universe_->frame, args->update);
    }
    qbResult result = AS_PRIVATE(loop());
    coro_scheduler->run_sync();    

    qb_timer_add(update_timer);

    game_loop.accumulator -= game_loop.dt;
    game_loop.t += game_loop.dt;
  }

  qb_timer_start(render_timer);

  qbRenderEvent_ e;
  e.frame = universe_->frame;
  e.alpha = game_loop.accumulator / game_loop.dt;
  e.camera = qb_camera_active();
  e.renderer = qb_renderer();

  if (callbacks && callbacks->on_prerender) {
    callbacks->on_prerender(&e, args->prerender);
  }

  gui_block_updateuniforms();
  if (e.camera) {
    qb_render(&e);
  }

  if (callbacks && callbacks->on_postrender) {
    callbacks->on_postrender(&e, args->postrender);
  }

  qb_timer_add(render_timer);
  double elapsed_render = qb_timer_average(render_timer) * 1e-9 + extra_render_time;
  if (elapsed_render < 10e-9) {
    elapsed_render += game_loop.dt;
  }
  game_loop.accumulator += elapsed_render;

  ++universe_->frame;
  qb_timer_stop(fps_timer);

  auto update_timer_avg = qb_timer_average(update_timer);
  auto render_timer_avg = qb_timer_average(render_timer);
  auto fps_timer_elapsed = qb_timer_elapsed(fps_timer);

  double update_fps = update_timer_avg == 0 ? 0 : game_loop.kClockResolution / update_timer_avg;
  double render_fps = render_timer_avg == 0 ? 0 : game_loop.kClockResolution / render_timer_avg;
  double total_fps = fps_timer_elapsed == 0 ? 0 : game_loop.kClockResolution / fps_timer_elapsed;

  timing_info.frame = universe_->frame;
  timing_info.render_fps = render_fps;
  timing_info.total_fps = total_fps;
  timing_info.udpate_fps = update_fps;

  return game_loop.is_running ? QB_OK : QB_DONE;
}

qbResult qb_loop(qbLoopCallbacks callbacks,
                 qbLoopArgs args) {
  if (!game_loop.is_running) {
    return QB_DONE;
  }

  if (universe_->enabled & QB_FEATURE_GAME_LOOP) {
    return loop(callbacks, args);
  } else {
    qbResult result = AS_PRIVATE(loop());
    coro_scheduler->run_sync();
    return result;
  }
}

qbResult qb_timing(qbUniverse universe, qbTiming timing) {
  *timing = timing_info;
  return QB_OK;
}

qbId qb_create_program(const char* name) {
  return AS_PRIVATE(create_program(name));
}

qbResult qb_run_program(qbId program) {
  return AS_PRIVATE(run_program(program));
}

qbResult qb_detach_program(qbId program) {
  return AS_PRIVATE(detach_program(program));
}

qbResult qb_join_program(qbId program) {
  return AS_PRIVATE(join_program(program));
}

qbResult qb_system_enable(qbSystem system) {
  return AS_PRIVATE(enable_system(system));
}

qbResult qb_system_disable(qbSystem system) {
  return AS_PRIVATE(disable_system(system));
}

qbResult qb_componentattr_create(qbComponentAttr* attr) {
  *attr = (qbComponentAttr)calloc(1, sizeof(qbComponentAttr_));
  new (*attr) qbComponentAttr_;
  (*attr)->is_shared = false;
  (*attr)->type = qbComponentType::QB_COMPONENT_TYPE_RAW;
	return qbResult::QB_OK;
}

qbResult qb_componentattr_destroy(qbComponentAttr* attr) {
  delete *attr;
  *attr = nullptr;
	return qbResult::QB_OK;
}

qbResult qb_componentattr_setdatasize(qbComponentAttr attr, size_t size) {
  attr->data_size = size;
  return qbResult::QB_OK;
}

qbResult qb_componentattr_settype(qbComponentAttr attr, qbComponentType type) {
  attr->type = type;
	return qbResult::QB_OK;
}

qbResult qb_componentattr_setshared(qbComponentAttr attr) {
  attr->is_shared = true;
  return qbResult::QB_OK;
}

qbResult qb_componentattr_setschema(qbComponentAttr attr, qbSchema schema) {
  attr->schema = schema;
  return qbResult::QB_OK;
}

qbResult qb_component_create(
    qbComponent* component, const char* name, qbComponentAttr attr) {
  if (!name) {
    return QB_UNKNOWN;
  }

  if (name[0] == '\0') {
    return QB_UNKNOWN;
  }

  attr->name = STRDUP(name);
  return AS_PRIVATE(component_create(component, attr));
}

qbResult qb_component_destroy(qbComponent*) {
	return qbResult::QB_OK;
}

size_t qb_component_count(qbComponent component) {
  return AS_PRIVATE(component_getcount(component));
}

qbComponent qb_component_find(const char* name, qbSchema* schema) {
  if (!name) {
    return -1;
  }

  qbComponent ret = AS_PRIVATE(component_find(name));
  if (schema) {
    *schema = qb_component_schema(ret);
  }

  return ret;
}

qbSchema qb_component_schema(qbComponent component) {
  return AS_PRIVATE(component_schema(component));
}

qbResult qb_entityattr_create(qbEntityAttr* attr) {
  *attr = (qbEntityAttr)calloc(1, sizeof(qbEntityAttr_));
  new (*attr) qbEntityAttr_;
	return qbResult::QB_OK;
}

qbResult qb_entityattr_destroy(qbEntityAttr* attr) {
  delete *attr;
  *attr = nullptr;
	return qbResult::QB_OK;
}

qbResult qb_entityattr_addcomponent(qbEntityAttr attr, qbComponent component,
                                    void* instance_data) {
  attr->component_list.push_back({component, instance_data});
	return qbResult::QB_OK;
}

qbResult qb_entity_create(qbEntity* entity, qbEntityAttr attr) {
  return AS_PRIVATE(entity_create(entity, *attr));
}

qbResult qb_entity_destroy(qbEntity entity) {
  return AS_PRIVATE(entity_destroy(entity));
}

bool qb_entity_hascomponent(qbEntity entity, qbComponent component) {
  return AS_PRIVATE(entity_hascomponent(entity, component));
}

qbResult qb_entity_addcomponent(qbEntity entity, qbComponent component,
                                void* instance_data) {
  return AS_PRIVATE(entity_addcomponent(entity, component, instance_data));
}

qbResult qb_entity_removecomponent(qbEntity entity, qbComponent component) {
  return AS_PRIVATE(entity_removecomponent(entity, component));
}

qbId qb_entity_getid(qbEntity entity) {
  return entity;
}

qbResult qb_barrier_create(qbBarrier* barrier) {
  *barrier = AS_PRIVATE(barrier_create());
  return QB_OK;
}

qbResult qb_barrier_destroy(qbBarrier* barrier) {
  AS_PRIVATE(barrier_destroy(*barrier));
  return QB_OK;
}

qbResult qb_systemattr_create(qbSystemAttr* attr) {
  *attr = (qbSystemAttr)calloc(1, sizeof(qbSystemAttr_));
  new (*attr) qbSystemAttr_;
	return qbResult::QB_OK;
}

qbResult qb_systemattr_destroy(qbSystemAttr* attr) {
  delete *attr;
  *attr = nullptr;
	return qbResult::QB_OK;
}

qbResult qb_systemattr_setprogram(qbSystemAttr attr, qbId program) {
  attr->program = program;
	return qbResult::QB_OK;
}

qbResult qb_systemattr_addconst(qbSystemAttr attr, qbComponent component) {
  attr->constants.push_back(component);
  attr->components.push_back(component);
	return qbResult::QB_OK;
}

qbResult qb_systemattr_addmutable(qbSystemAttr attr, qbComponent component) {
  attr->mutables.push_back(component);
  attr->components.push_back(component);
	return qbResult::QB_OK;
}

qbResult qb_systemattr_setfunction(qbSystemAttr attr, qbTransformFn transform) {
  attr->transform = transform;
	return qbResult::QB_OK;
}

qbResult qb_systemattr_setcallback(qbSystemAttr attr, qbCallbackFn callback) {
  attr->callback = callback;
	return qbResult::QB_OK;
}

qbResult qb_systemattr_setcondition(qbSystemAttr attr, qbConditionFn condition) {
  attr->condition = condition;
  return qbResult::QB_OK;
}

qbResult qb_systemattr_settrigger(qbSystemAttr attr, qbTrigger trigger) {
  attr->trigger = trigger;
	return qbResult::QB_OK;
}


qbResult qb_systemattr_setpriority(qbSystemAttr attr, int16_t priority) {
  attr->priority = priority;
	return qbResult::QB_OK;
}

qbResult qb_systemattr_setjoin(qbSystemAttr attr, qbComponentJoin join) {
  attr->join = join;
	return qbResult::QB_OK;
}

qbResult qb_systemattr_setuserstate(qbSystemAttr attr, void* state) {
  attr->state = state;
	return qbResult::QB_OK;
}

qbResult qb_systemattr_addbarrier(qbSystemAttr attr,
                                  qbBarrier barrier) {
  qbTicket_* t = new qbTicket_;
  t->impl = ((Barrier*)barrier->impl)->MakeTicket().release();

  t->lock = [ticket{ t->impl }]() { ((Barrier::Ticket*)ticket)->lock(); };
  t->unlock = [ticket{ t->impl }]() { ((Barrier::Ticket*)ticket)->unlock(); };
  attr->tickets.push_back(t);
  return QB_OK;
}

qbResult qb_system_create(qbSystem* system, qbSystemAttr attr) {
  if (!attr->program) {
    attr->program = 0;
  }
#ifdef __ENGINE_DEBUG__
  DEBUG_ASSERT(attr->transform || attr->callback,
               qbResult::QB_ERROR_SYSTEMATTR_HAS_FUNCTION_OR_CALLBACK);
#endif
  AS_PRIVATE(system_create(system, *attr));
	return qbResult::QB_OK;
}

qbResult qb_system_destroy(qbSystem*) {
	return qbResult::QB_OK;
}

qbResult qb_system_foreach(qbComponent* components, size_t component_count,
                           qbVar state, void(*fn)(qbInstance*, qbVar)) {
  return AS_PRIVATE(foreach_system(components, component_count, state, fn));
}

qbResult qb_eventattr_create(qbEventAttr* attr) {
  *attr = (qbEventAttr)calloc(1, sizeof(qbEventAttr_));
  new (*attr) qbEventAttr_;
	return qbResult::QB_OK;
}

qbResult qb_eventattr_destroy(qbEventAttr* attr) {
  delete *attr;
  *attr = nullptr;
	return qbResult::QB_OK;
}

qbResult qb_eventattr_setprogram(qbEventAttr attr, qbId program) {
  attr->program = program;
	return qbResult::QB_OK;
}

qbResult qb_eventattr_setmessagesize(qbEventAttr attr, size_t size) {
  attr->message_size = size;
	return qbResult::QB_OK;
}

qbResult qb_event_create(qbEvent* event, qbEventAttr attr) {
  if (!attr->program) {
    attr->program = 0;
  }
#ifdef __ENGINE_DEBUG__
  DEBUG_ASSERT(attr->message_size > 0,
               qbResult::QB_ERROR_EVENTATTR_MESSAGE_SIZE_IS_ZERO);
#endif
	return AS_PRIVATE(event_create(event, attr));
}

qbResult qb_event_destroy(qbEvent* event) {
	return AS_PRIVATE(event_destroy(event));
}

qbResult qb_event_flushall(qbProgram program) {
	return AS_PRIVATE(event_flushall(program));
}

qbResult qb_event_subscribe(qbEvent event, qbSystem system) {
	return AS_PRIVATE(event_subscribe(event, system));
}

qbResult qb_event_unsubscribe(qbEvent event, qbSystem system) {
	return AS_PRIVATE(event_unsubscribe(event, system));
}

qbResult qb_event_send(qbEvent event, void* message) {
  return AS_PRIVATE(event_send(event, message));
}

qbResult qb_event_sendsync(qbEvent event, void* message) {
  return AS_PRIVATE(event_sendsync(event, message));
}

qbResult qb_instance_oncreate(qbComponent component,
                              qbInstanceOnCreate on_create) {
  return AS_PRIVATE(instance_oncreate(component, on_create));
}

qbResult qb_instance_ondestroy(qbComponent component,
                               qbInstanceOnDestroy on_destroy) {
  return AS_PRIVATE(instance_ondestroy(component, on_destroy));
}

qbEntity qb_instance_entity(qbInstance instance) {
  return instance->entity;
}

qbResult qb_instance_const(qbInstance instance, void* pbuffer) {
  return AS_PRIVATE(instance_getconst(instance, pbuffer));
}

qbResult qb_instance_mutable(qbInstance instance, void* pbuffer) {
  return AS_PRIVATE(instance_getmutable(instance, pbuffer));
}

qbResult qb_instance_component(qbInstance instance, qbComponent component, void* pbuffer) {
  return AS_PRIVATE(instance_getcomponent(instance, component, pbuffer));
}

bool qb_instance_hascomponent(qbInstance instance, qbComponent component) {
  return AS_PRIVATE(instance_hascomponent(instance, component));
}

qbResult qb_instance_find(qbComponent component, qbEntity entity, void* pbuffer) {
  return AS_PRIVATE(instance_find(component, entity, pbuffer));
}

qbRef qb_instance_at(qbInstance instance, const char* key) {
  qbStruct_* s = (qbStruct_*)instance->data;

  if (!key) {
    return{ QB_TAG_PTR, (void*)&s->data };
  }

  for (const auto& f : s->internals.schema->fields) {
    if (f.key == key) {
      qbRef r;
      r.tag = f.tag;
      if (r.tag == QB_TAG_INT) {
        r.i = (int64_t*)(&s->data + f.offset);
      } else if (r.tag == QB_TAG_DOUBLE) {
        r.d = (double*)(&s->data + f.offset);
      } else {
        memcpy(&r.bytes, &s->data + f.offset, f.size);
      }
      return r;
    }
  }

  return{ QB_TAG_NIL, 0 };
}

qbCoro qb_coro_create(qbVar(*entry)(qbVar var)) {
  qbCoro ret = new qbCoro_();
  ret->ret = qbFuture;
  ret->main = coro_new(entry);
  return ret;
}

qbCoro qb_coro_copy(qbCoro coro) {
  qbCoro ret = new qbCoro_();
  ret->ret = qbFuture;
  ret->main = coro_clone(coro->main);
  return ret;
}

qbResult qb_coro_destroy(qbCoro* coro) {
  coro_free((*coro)->main);
  delete *coro;
  *coro = nullptr;
  return QB_OK;
}

qbVar qb_coro_call(qbCoro coro, qbVar var) {
  coro->arg = var;
  return coro_call(coro->main, var);
}

qbCoro qb_coro_sync(qbVar(*entry)(qbVar), qbVar var) {
  return coro_scheduler->schedule_sync(entry, var);
}

qbCoro qb_coro_async(qbVar(*entry)(qbVar), qbVar var) {
  return coro_scheduler->schedule_async(entry, var);
}

qbVar qb_coro_await(qbCoro coro) {
  return coro_scheduler->await(coro);
}

qbVar qb_coro_peek(qbCoro coro) {
  return coro_scheduler->peek(coro);
}

qbVar qb_coro_yield(qbVar var) {
  return coro_yield(var);
}

void qb_coro_wait(double seconds) {
  if (seconds == 0) {
    seconds = 1e-9;
  }
  double start = (double)qb_timer_query() / 1e9;
  double end = start + seconds;
  while ((double)qb_timer_query() / 1e9 < end) {
    qb_coro_yield(qbFuture);
  }
}

void qb_coro_waitframes(uint32_t frames) {
  volatile uint32_t frames_waited = 0;
  while (frames_waited < frames) {
    qb_coro_yield(qbFuture);
    ++frames_waited;
  }
}

bool qb_coro_done(qbCoro coro) {
  return qb_coro_peek(coro).tag != QB_TAG_NIL;
}

qbVar qbPtr(void* p) {
  qbVar v;
  v.tag = QB_TAG_PTR;
  v.p = p;
  return v;
}

qbVar qbInt(int64_t i) {
  qbVar v;
  v.tag = QB_TAG_INT;
  v.i = i;
  return v;
}

qbVar qbDouble(double d) {
  qbVar v;
  v.tag = QB_TAG_DOUBLE;
  v.d = d;
  return v;
}

qbVar qbStruct(qbSchema schema, void* buf) {
  if (!buf) {
    buf = malloc(sizeof(qbStructInternals_) + schema->size);
  }

  qbVar v;
  v.p = buf;
  v.tag = QB_TAG_STRUCT;
  return v;
}

qbRef qb_var_at(qbVar v, const char* key) {
  qbStruct_* s = (qbStruct_*)v.p;

  if (!key) {
    return{ QB_TAG_PTR, (void*)&s->data };
  }

  for (const auto& f : s->internals.schema->fields) {
    if (f.key == key) {
      qbRef r;
      r.tag = f.tag;
      if (r.tag == QB_TAG_INT) {
        r.i = (int64_t*)(&s->data + f.offset);
      } else if (r.tag == QB_TAG_DOUBLE) {
        r.d = (double*)(&s->data + f.offset);
      } else {
        memcpy(&r.bytes, &s->data + f.offset, f.size);
      }
      return r;
    }
  }

  return{ QB_TAG_NIL, 0 };
}

qbResult qb_scene_create(qbScene* scene, const char* name) {
  return AS_PRIVATE(scene_create(scene, name));
}

qbResult qb_scene_destroy(qbScene* scene) {
  return AS_PRIVATE(scene_destroy(scene));
}

qbScene qb_scene_global() {
  return AS_PRIVATE(scene_global());
}

qbResult qb_scene_save(qbScene* scene, const char* file) {
  return QB_OK;
}

qbResult qb_scene_load(qbScene* scene, const char* name, const char* file) {
  return QB_OK;
}

qbResult qb_scene_set(qbScene scene) {
  return AS_PRIVATE(scene_set(scene));
}

const char* qb_scene_name(qbScene scene) {
  return scene->name;
}

qbResult qb_scene_reset() {
  return AS_PRIVATE(scene_reset());
}

qbResult qb_scene_activate(qbScene scene) {
  return AS_PRIVATE(scene_activate(scene));
}

qbResult qb_scene_attach(qbScene scene, const char* key, void* value) {
  return AS_PRIVATE(scene_attach(scene, key, value));
}

qbResult qb_scene_ondestroy(qbScene scene, void(*fn)(qbScene scene,
                                                     size_t count,
                                                     const char* keys[],
                                                     void* values[])) {
  return AS_PRIVATE(scene_ondestroy(scene, fn));
}

qbResult qb_scene_onactivate(qbScene scene, void(*fn)(qbScene scene,
                                                      size_t count,
                                                      const char* keys[],
                                                      void* values[])) {
  return AS_PRIVATE(scene_onactivate(scene, fn));
}

qbResult qb_scene_ondeactivate(qbScene scene, void(*fn)(qbScene scene,
                                                        size_t count,
                                                        const char* keys[],
                                                        void* values[])) {
  return AS_PRIVATE(scene_ondeactivate(scene, fn));
}

qbSchema qb_schema_find(const char* name) {
  return AS_PRIVATE(schema_find(name));
}

size_t qb_schema_size(qbSchema schema) {
  return schema->size;
}

size_t qb_struct_size(qbSchema schema) {
  return sizeof(qbStructInternals_) + schema->size;
}

size_t qb_schema_fields(qbSchema schema, qbSchemaField* fields) {
  *fields = schema->fields.data();
  return schema->fields.size();
}

const char* qb_schemafield_name(qbSchemaField field) {
  return field->key.c_str();
}

qbTag qb_schemafield_tag(qbSchemaField field) {
  return field->tag;
}

size_t qb_schemafield_offset(qbSchemaField field) {
  return field->offset;
}

size_t qb_schemafield_size(qbSchemaField field) {
  return field->size;
}