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
#include <cubez/draw.h>
#include <cubez/time.h>
#include <cubez/random.h>
#include <cubez/renderer.h>
#include <cubez/audio.h>
#include <cubez/socket.h>
#include <cubez/struct.h>
#include <cubez/input.h>
#include <filesystem>
#include <iomanip>
#include <locale>
#include <string>
#include <codecvt>

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
#include "audio_internal.h"
#include "network_impl.h"
#include "lua_bindings.h"
#include "utf8.h"
#include "memory_internal.h"
#include "async_internal.h"
#include "sprite_internal.h"
#include "entity_table.h"

namespace fs = std::filesystem;

#define AS_PRIVATE(expr) ((PrivateUniverse*)(universe_->self))->expr

const qbVar qbNil = { QB_TAG_NIL, 0, 0 };
const qbVar qbFuture = { QB_TAG_FUTURE, 0, 0 };
const qbEntity qbInvalidEntity = -1;
const qbHandle qbInvalidHandle = -1;
const qbComponent qbInvalidComponent = -1;

static qbUniverse* universe_ = nullptr;
qbTiming_ timing_info;
std::vector<double> elapsed_update_samples;

qbTimer fps_timer;
qbTimer update_timer;
qbTimer render_timer;
CoroScheduler* coro_scheduler;
Coro coro_main;

std::u8string cwd_dir_str;
fs::path cwd_dir;
fs::path resource_dir;
qbResourceAttr_ resource_attr{};

static qbComponent qb_id_component;

struct GameLoop {
  const double kClockResolution = 1e9;
  const double dt = 0.01;

  double t;
  double current_time;
  double start_time;
  double accumulator;
  volatile std::atomic_bool is_running{ false };
} game_loop;

namespace {
std::u8string wstring_to_utf8(const std::wstring& str) {
  std::wstring_convert<std::codecvt_utf16<wchar_t>> to_utf16;
  std::wstring_convert<std::codecvt_utf8_utf16<char16_t, 0x10ffff,
    std::codecvt_mode::little_endian>, char16_t> to_utf8;

  std::u16string utf16_str((char16_t*)to_utf16.to_bytes(str).data());
  std::string utf8_str = to_utf8.to_bytes(utf16_str);

  return std::u8string(utf8_str.begin(), utf8_str.end());
}
}

qbResult qb_init(qbUniverse* u, qbUniverseAttr attr) {
  universe_ = u;
  assert(u->argc > 0 && (u->argv || u->wargv) && "Must include the argc and argv during initialization.");

  u->enabled = attr->enabled;

  memory_initialize();
  utils_initialize();
  coro_main = coro_initialize(u);

  {
    std::filesystem::path cwd;

    if (__argv) {
      std::u8string arg(u->argv[0], u->argv[0] + strlen(u->argv[0]));
      cwd = arg;
    } else {
      std::wstring warg(u->wargv[0]);
      std::u8string arg = wstring_to_utf8(warg);
      cwd = arg;
    }
    cwd = cwd.remove_filename();

    cwd_dir_str = cwd.u8string();
    cwd_dir = cwd.u8string().c_str();

    std::filesystem::path resource_path = cwd;

    resource_attr.resources = u8"resources";
    resource_attr.fonts = u8"";
    resource_attr.scripts = u8"";
    resource_attr.sounds = u8"";
    resource_attr.images = u8"";

    if (attr->resource_args) {
      resource_attr.resources = attr->resource_args->resources ?
        (const utf8_t*)STRDUP((const char*)attr->resource_args->resources) : (const utf8_t*)resource_attr.resources;

      resource_attr.fonts = attr->resource_args->fonts ?
        (const utf8_t*)STRDUP((const char*)attr->resource_args->fonts) : (const utf8_t*)resource_attr.fonts;

      resource_attr.scripts = attr->resource_args->scripts ?
        (const utf8_t*)STRDUP((const char*)attr->resource_args->scripts) : (const utf8_t*)resource_attr.scripts;

      resource_attr.sounds = attr->resource_args->sounds ?
        (const utf8_t*)STRDUP((const char*)attr->resource_args->sounds) : (const utf8_t*)resource_attr.sounds;

      resource_attr.images = attr->resource_args->images ?
        (const utf8_t*)STRDUP((const char*)attr->resource_args->images) : (const utf8_t*)resource_attr.images;
    }

    resource_dir = resource_path / fs::path(resource_attr.resources);
  }
  
  // Initialize Lua before PrivateUniverse is constructed because the Lua VM
  // is initialized per-thread.
  {
    qbScriptAttr_ empty_args = {};
    lua_bindings_initialize(attr->script_args ? attr->script_args : &empty_args);
  }

  universe_->self = new PrivateUniverse();
  coro_scheduler = new CoroScheduler(attr->scheduler_args ? attr->scheduler_args->max_async_coros : 16);
  async_initialize(attr->scheduler_args);  

  qbResult ret = AS_PRIVATE(init());

  if (universe_->enabled == QB_FEATURE_ALL) {
    universe_->enabled = 0xFFFF;
  }
  if (universe_->enabled & QB_FEATURE_LOGGER) {
    qbLoggingAttr_ logs_attr = {
      .logs = u8"logs"
    };

    if (attr->logging_args) {
      logs_attr.logs = attr->logging_args->logs ?
        (const utf8_t*)STRDUP((const char*)attr->logging_args->logs) : logs_attr.logs;
    }

    log_initialize(logs_attr);
  }
  if (universe_->enabled & QB_FEATURE_INPUT) {
    input_initialize();
  }
  if (universe_->enabled & QB_FEATURE_GRAPHICS) {
    RenderSettings render_settings{};
    render_settings.title = attr->title;
    render_settings.width = attr->width;
    render_settings.height = attr->height;

    qbRendererAttr_ empty_args = {};
    if (attr->renderer_args) {
      render_settings.create_renderer = attr->renderer_args->create_renderer;
      render_settings.destroy_renderer = attr->renderer_args->destroy_renderer;
    }
    render_settings.opt_renderer_args = attr->renderer_args ? attr->renderer_args : &empty_args;

    render_initialize(&render_settings);    
  }

  if (universe_->enabled & QB_FEATURE_AUDIO) {
    audio_initialize(attr->audio_args);
  }

  if (universe_->enabled & QB_FEATURE_GAME_LOOP) {
    qb_timer_create(&fps_timer, 50);
    qb_timer_create(&update_timer, 50);
    qb_timer_create(&render_timer, 50);
  }

  network_initialize();

  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, uint64_t);
    qb_component_create(&qb_id_component, "id", attr);
    qb_componentattr_destroy(&attr);
  }

  EntityTable::Initialize();

  return ret;
}

qbResult qb_start() {
  game_loop.t = 0.0;
  game_loop.current_time = qb_time() * 0.000000001;
  game_loop.start_time = (double)qb_time();
  game_loop.accumulator = 0.0;
  game_loop.is_running = true;
  return AS_PRIVATE(start());
}

qbResult qb_stop() {
  if (!game_loop.is_running) return QB_DONE;

  game_loop.is_running = false;
  qbResult ret = AS_PRIVATE(stop());

  async_stop();
  delete coro_scheduler;

  network_shutdown();
  audio_shutdown();
  render_shutdown();
    
  universe_ = nullptr;
  return ret;
}

const utf8_t* qb_dir() {
  return cwd_dir_str.c_str();
}

qbBool qb_running() {
  return game_loop.is_running;
}

const qbResourceAttr_* qb_resources() {
  return &resource_attr;
}

qbResult loop(qbLoopCallbacks callbacks,
              qbLoopArgs args) {

  qbLoopCallbacks_ empty_callbacks{};
  qbLoopArgs_ empty_args{};
  if (!callbacks) {
    callbacks = &empty_callbacks;
  }

  if (!args) {
    args = &empty_args;
  }

  elapsed_update_samples.clear();

  qb_timer_start(fps_timer);

  const static double extra_render_time = universe_->enabled & QB_FEATURE_GRAPHICS ? 0.0 : game_loop.dt;
  double new_time = qb_time() * 0.000000001;
  double frame_time = new_time - game_loop.current_time;
  frame_time = std::min(0.25, frame_time);
  game_loop.current_time = new_time;

  // TODO: should this or shouldn't done per frame?
  // i.e. if (game_loop.accumulator >= game_loop.dt) or not
  // But Nuklear needs to have this once per frame to clear the input state per frame.
  {
    struct ResizeState {
      qbLoopCallbacks callbacks;
      qbLoopArgs args;
    } resize_state{ callbacks, args };

    qb_handle_input([](qbVar) {
      qb_stop();
    }, [](qbVar arg, uint32_t width, uint32_t height) {      
      ResizeState* resize_state = (ResizeState*)arg.p;
      lua_resize(AS_PRIVATE(main_lua_state()), width, height);

      if (resize_state->callbacks->on_resize) {
        resize_state->callbacks->on_resize(width, height, arg);
      }

      qbRenderer r = qb_renderer();
      if (r) {
        r->resize(r, width, height);
      }
    }, qbNil, qbPtr(&resize_state));

    if (!game_loop.is_running) {
      return QB_DONE;
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

    elapsed_update_samples.push_back((double)qb_timer_add(update_timer));

    game_loop.accumulator -= game_loop.dt;
    game_loop.t += game_loop.dt;
  }

  qb_timer_start(render_timer);

  qbRenderEvent_ e;
  e.frame = universe_->frame;
  e.alpha = game_loop.accumulator / game_loop.dt;
  e.dt = frame_time;
  e.renderer = qb_renderer();

  if (universe_->enabled & QB_FEATURE_GRAPHICS) {
    //gui_element_updateuniforms();
  }

  lua_draw(AS_PRIVATE(main_lua_state()));
  do_render(&e, callbacks->on_render, callbacks->on_postrender, args->render, args->postrender);

  timing_info.render_elapsed_ns = qb_timer_add(render_timer);
  double elapsed_render = qb_timer_average(render_timer) * 1e-9 + extra_render_time;
  if (elapsed_render < 10e-9) {
    elapsed_render += game_loop.dt;
  }
  game_loop.accumulator += elapsed_render;

  memory_flush_all(universe_->frame);

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
  timing_info.total_elapsed_ns = fps_timer_elapsed;  
  timing_info.update_elapsed_samples = elapsed_update_samples.data();
  timing_info.update_elapsed_samples_count = elapsed_update_samples.size();

  return game_loop.is_running ? QB_OK : QB_DONE;
}

qbResult qb_loop(qbLoopCallbacks callbacks,
                 qbLoopArgs args) {
  qbLoopCallbacks_ empty_callbacks{};
  qbLoopArgs_ empty_args{};
  if (!callbacks) {
    callbacks = &empty_callbacks;
  }

  if (!args) {
    args = &empty_args;
  }

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

uint64_t qb_framenum() {
  return timing_info.frame;
}

qbId qb_program_create(const char* name) {
  return AS_PRIVATE(create_program(name));
}

void qb_program_onready(qbId program,
                        void(*onready)(qbProgram* program, qbVar), qbVar state) {
  AS_PRIVATE(onready_program(program, onready, state));
}

qbResult qb_program_run(qbId program) {
  return AS_PRIVATE(run_program(program));
}

qbResult qb_program_detach(qbId program) {
  return AS_PRIVATE(detach_program(program));
}

qbResult qb_program_join(qbId program) {
  return AS_PRIVATE(join_program(program));
}

struct lua_State* qb_luastate() {
  return AS_PRIVATE(main_lua_state());
}

qbResult qb_system_enable(qbSystem system) {
  return AS_PRIVATE(enable_system(system));
}

qbResult qb_system_disable(qbSystem system) {
  return AS_PRIVATE(disable_system(system));
}

qbVar qb_system_run(qbSystem system, qbVar arg) {
  return AS_PRIVATE(run_system(system, arg));
}

qbResult qb_componentattr_create(qbComponentAttr* attr) {
  *attr = (qbComponentAttr)calloc(1, sizeof(qbComponentAttr_));
  new (*attr) qbComponentAttr_;
  (*attr)->is_shared = true;
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

qbResult qb_componentattr_setshared(qbComponentAttr attr, qbBool is_shared) {
  attr->is_shared = is_shared == QB_TRUE;
  return qbResult::QB_OK;
}

qbResult qb_componentattr_onpack(
  qbComponentAttr attr,
  size_t(*fn)(qbComponent component, const void* read, qbBuffer_* write, ptrdiff_t* pos)) {

  attr->onpack = fn;
  return qbResult::QB_OK;
}

qbResult qb_componentattr_onunpack(
  qbComponentAttr attr,
  size_t(*fn)(qbComponent component, const void* read, qbBuffer_* write, ptrdiff_t* pos)) {
  
  attr->onunpack = fn;
  return qbResult::QB_OK;
}

qbResult qb_componentattr_setschema(qbComponentAttr attr, qbSchema schema) {
  attr->schema = schema;
  return qbResult::QB_OK;
}

size_t pack_struct(qbSchema schema, const void* read, qbBuffer_* buf, ptrdiff_t* pos) {
  size_t written = 0;
  for (size_t i = 0; i < schema->fields.size(); ++i) {
    const auto& field = schema->fields[i];
    const char* data = &((qbStruct_*)read)->data + field.offset;

    switch (field.type.tag) {
      case QB_TAG_INT:
      case QB_TAG_UINT:
      case QB_TAG_DOUBLE:
      case QB_TAG_BYTES:
        qb_buffer_write(buf, pos, field.size, data);
        break;

      case QB_TAG_CSTRING:
      case QB_TAG_STRING:
      {
        qbStr_* s = *(qbStr_**)data;
        qb_buffer_write(buf, pos, s->len, s->bytes);
        break;
      }

      case QB_TAG_STRUCT:
        written += pack_struct(schema, data, buf, pos);
        break;
    }
  }

  return written;
}

qbResult qb_component_create(
    qbComponent* component, const char* name, qbComponentAttr attr) {
  if (!name) {
    return QB_UNKNOWN;
  }

  if (name[0] == '\0') {
    return QB_UNKNOWN;
  }

  if (attr->type == QB_COMPONENT_TYPE_SCHEMA) {
    if (!attr->onpack) {
      attr->onpack = [](qbComponent component, const void* read, qbBuffer_* write, ptrdiff_t* pos) {
        static qbSchema schema = qb_component_schema(component);
        return pack_struct(schema, read, write, pos);
      };
    }
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
  if (ret >= 0 && schema) {
    *schema = qb_component_schema(ret);
  }

  return ret;
}

qbSchema qb_component_schema(qbComponent component) {
  return AS_PRIVATE(component_schema(component));
}

size_t qb_component_size(qbComponent component) {
  return size_t();
}

size_t qb_component_pack(qbComponent component, const qbBuffer_* read,
                         qbBuffer_* write, ptrdiff_t* pos) {
  return AS_PRIVATE(component_pack(component, read, write, pos));
}

size_t qb_component_unpack(qbComponent component, const qbBuffer_* read,
                           qbBuffer_* write, ptrdiff_t* pos) {
  return AS_PRIVATE(component_unpack(component, read, write, pos));
}

qbResult qb_component_oncreate(qbComponent component, qbEventFn fn, qbVar arg) {
  return AS_PRIVATE(component_oncreate(component, fn, arg));
}

qbResult qb_component_ondestroy(qbComponent component, qbEventFn fn, qbVar arg) {
  return AS_PRIVATE(component_ondestroy(component, fn, arg));
}

qbResult qb_entityattr_create(qbEntityAttr* attr) {
  *attr = (qbEntityAttr)calloc(1, sizeof(qbEntityAttr_));
  new (*attr) qbEntityAttr_;
	return qbResult::QB_OK;
}

size_t qb_entityattr_size() {
  return sizeof(qbEntityAttr_);
}

qbResult qb_entityattr_init(qbEntityAttr* attr) {
  new(*attr)(qbEntityAttr_){};
  return QB_OK;
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
  qbEntityAttr_ empty{};

  if (!attr) {
    attr = &empty;
  }  

  qbResult result = AS_PRIVATE(entity_create(entity, *attr));
  if (result != QB_OK) {
    return result;
  }

  uint64_t id = qb_rand();
  qb_entity_addcomponent(*entity, qb_uid(), &id);

  return QB_OK;
}
qbEntity qb_entity_withlen(size_t count, const qbComponentData_ data[]) {
  qbEntity ret;
  AS_PRIVATE(entity_create(&ret, count, data));

  uint64_t id = qb_rand();
  qb_entity_addcomponent(ret, qb_uid(), &id);

  return ret;
}

uint64_t qb_entity_id(qbEntity entity) {
  void* ret = qb_entity_getcomponent(entity, qb_uid());
  if (!ret) {
    return qbInvalidEntity;
  }

  return *(uint64_t*)ret;
}

qbEntity qb_entity_empty() {
  qbComponentData_ empty = {};
  return qb_entity_withlen(0, &empty);
}

qbResult qb_entity_destroy(qbEntity entity) {
  return AS_PRIVATE(entity_destroy(entity));
}

qbBool qb_entity_hascomponent(qbEntity entity, qbComponent component) {
  return AS_PRIVATE(entity_hascomponent(entity, component));
}

void* qb_entity_getcomponent(qbEntity entity, qbComponent component) {
  return AS_PRIVATE(entity_getcomponent(entity, component));
}

qbResult qb_entity_addcomponent(qbEntity entity, qbComponent component,
                                void* instance_data) {
  return AS_PRIVATE(entity_addcomponent(entity, component, instance_data));
}

qbResult qb_entity_addcomponents(qbEntity entity, size_t count, const qbComponentData_ data[]) {
  return AS_PRIVATE(entity_addcomponents(entity, count, data));
}

qbResult qb_entity_removecomponent(qbEntity entity, qbComponent component) {
  return AS_PRIVATE(entity_removecomponent(entity, component));
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

qbResult qb_system_foreach(size_t component_count, qbComponent components[],
                           qbVar state, void(*fn)(qbInstance, qbVar)) {
  return AS_PRIVATE(foreach_system(components, component_count, state, fn));
}

qbResult qb_query(qbQuery query, qbVar arg) {
  return AS_PRIVATE(do_query(query, arg));
}

qbIterator_ qb_component_iterate_(qbComponent component, ...) {
  va_list components;
  va_start(components, component);

  qbIterator_ iterator = {0};
  AS_PRIVATE(component_iterate(component, (qbIteratorImpl_*)&iterator, components));
  
  va_end(components);
  return iterator;
}

qbBool qb_iterator_next(qbIterator it) {
  return AS_PRIVATE(iterator_next(it));
}

void qb_iterator_get_(qbIterator it, ...) {
  va_list args;
  va_start(args, it);

  AS_PRIVATE(iterator_get(it, args));

  va_end(args);
}

void qb_iterator_getn(qbIterator it, size_t count, void* pbufs[]) {
  AS_PRIVATE(iterator_get(it, count, pbufs));
}

qbEntity qb_iterator_entity(qbIterator it) {
  return AS_PRIVATE(iterator_entity(it));
}

void qb_iterator_index(qbIterator it, size_t index, void* pbuf) {
  AS_PRIVATE(iterator_index(it, index, pbuf));
}

qbBool qb_iterator_component(qbIterator it, qbComponent component, void* pbuf) {
  return AS_PRIVATE(iterator_component(it, component, pbuf));
}

qbResult qb_entitytableattr_create(qbEntityTableAttr* attr) {
  *attr = new qbEntityTableAttr_{};
  return QB_OK;
}

qbResult qb_entitytableattr_destroy(qbEntityTableAttr* attr) {
  delete *attr;
  *attr = nullptr;
  return QB_OK;
}

qbResult qb_entitytableattr_add(qbEntityTableAttr attr, qbComponent component) {
  attr->components.push_back(component);
  return QB_OK;
}

qbResult qb_entitytable_create(qbEntityTable* table, qbEntityTableAttr attr) {
  return AS_PRIVATE(table_create(table, attr));
}

qbResult qb_entitytable_destroy(qbEntityTable* table) {
  return AS_PRIVATE(table_destroy(table));
}

qbComponent qb_entitytable_component() {
  return EntityTable::Component();
}

qbIterator_ qb_entitytable_iterate_(qbEntityTable table, ...) {
  va_list args;
  va_start(args, table);

  qbIterator_ it{};
  AS_PRIVATE(table_iterate(table, (qbIteratorImpl_*)&it, args));
  
  va_end(args);

  return it;
}

qbIterator_ qb_entitytable_iteraten(qbEntityTable table, size_t count, qbComponent components[]) {
  qbIterator_ it{};
  AS_PRIVATE(table_iterate(table, (qbIteratorImpl_*)&it, count, components));

  return it;
}

qbEntity qb_entitytable_insert_(qbEntityTable table, ...) {
  va_list args;
  va_start(args, table);

  EntityTable* impl = EntityTable::FromRaw(table);
  qbEntity ret = impl->insert(args);

  va_end(args);
  return ret;
}

qbEntity qb_entitytable_insertn(qbEntityTable table, size_t count, void* pbufs[]) {
  EntityTable* impl = EntityTable::FromRaw(table);
  return impl->insert(count, pbufs);
}

void qb_entitytable_reserve(qbEntityTable table, size_t count) {
  EntityTable::FromRaw(table)->reserve(count);
}

void qb_entitytable_erase(qbEntityTable table, qbEntity entity) {
  EntityTable::FromRaw(table)->erase(entity);
}

void qb_entitytable_clear(qbEntityTable table) {
  return EntityTable::FromRaw(table)->erase_all();
}

size_t qb_entitytable_count(qbEntityTable table) {
  return EntityTable::FromRaw(table)->count();
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
#ifdef __ENGINE_DEBUG__
  DEBUG_ASSERT(attr->message_size > 0,
               qbResult::QB_ERROR_EVENTATTR_MESSAGE_SIZE_IS_ZERO);
#endif
	return AS_PRIVATE(event_create(event, attr));
}

qbResult qb_event_destroy(qbEvent* event) {
	return AS_PRIVATE(event_destroy(event));
}

qbResult qb_event_subscribe(qbEvent event, qbEventFn fn, qbVar arg) {
	return AS_PRIVATE(event_subscribe(event, fn, arg));
}

qbResult qb_event_unsubscribe(qbEvent event, qbEventFn fn) {
	return AS_PRIVATE(event_unsubscribe(event, fn));
}

qbResult qb_event_send(qbEvent event, void* message) {
  return AS_PRIVATE(event_send(event, message));
}

qbResult qb_event_sendsync(qbEvent event, void* message) {
  return AS_PRIVATE(event_sendsync(event, message));
}

qbResult qb_instance_oncreate(qbComponent component,
                              qbInstanceOnCreate on_create,
                              qbVar state) {
  return AS_PRIVATE(instance_oncreate(component, on_create, state));
}

qbResult qb_instance_ondestroy(qbComponent component,
                               qbInstanceOnDestroy on_destroy,
                               qbVar state) {
  return AS_PRIVATE(instance_ondestroy(component, on_destroy, state));
}

qbEntity qb_instance_entity(qbInstance instance) {
  return instance->entity;
}

qbResult qb_instance_component(qbInstance instance, qbComponent component, void* pbuffer) {
  return AS_PRIVATE(instance_getcomponent(instance, component, pbuffer));
}

qbBool qb_instance_hascomponent(qbInstance instance, qbComponent component) {
  return AS_PRIVATE(instance_hascomponent(instance, component));
}

void qb_instance_get_(qbInstance instance, ...) {
  va_list args;
  va_start(args, instance);
  AS_PRIVATE(instance_get(instance, args));
  va_end(args);
}

void qb_instance_geti(qbInstance instance, size_t index, void* pbuf) {
  AS_PRIVATE(instance_geti(instance, index, pbuf));
}

void qb_instance_getn(qbInstance instance, size_t count, void* pbufs[]) {
  AS_PRIVATE(instance_getn(instance, count, pbufs));
}

qbResult qb_instance_find(qbComponent component, qbEntity entity, void* pbuffer) {
  DEBUG_ASSERT(entity != qbInvalidEntity, 1);
  return AS_PRIVATE(instance_find(component, entity, pbuffer));
}

qbRef qb_instance_at(qbInstance instance, const char* key) {
  qbStruct_* s = (qbStruct_*)instance->data;
  qbVar var_s = qbStruct(s->internals.schema, s);
  return qb_ref_at(var_s, qbPtr((void*)key));
}

qbVar qb_instance_struct(qbInstance instance) {
  qbStruct_* s = (qbStruct_*)instance->data;
  return qbStruct(s->internals.schema, s);
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
  double start = (double)qb_time() / 1e9;
  double end = start + seconds;
  while ((double)qb_time() / 1e9 < end) {
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

qbBool qb_coro_done(qbCoro coro) {
  return qb_coro_peek(coro).tag == QB_TAG_NIL ? QB_FALSE : QB_TRUE;
}

qbVar qbPtr(void* p) {
  qbVar v{};
  v.tag = QB_TAG_PTR;
  v.size = sizeof(p);
  v.p = p;
  return v;
}

qbVar qbInt(int64_t i) {
  qbVar v{};
  v.tag = QB_TAG_INT;
  v.size = sizeof(i);
  v.i = i;
  return v;
}

qbVar qbUint(uint64_t u) {
  qbVar v{};
  v.tag = QB_TAG_UINT;
  v.size = sizeof(u);
  v.u = u;
  return v;
}

qbVar qbDouble(double d) {
  qbVar v{};
  v.tag = QB_TAG_DOUBLE;
  v.size = sizeof(d);
  v.d = d;
  return v;
}

qbVar qbStruct(qbSchema schema, void* buf) {
  qbVar v{};
  v.p = buf;
  v.tag = QB_TAG_STRUCT;
  v.size = schema->size;

  qbStruct_* s = (qbStruct_*)buf;
  s->internals.schema = schema;
  s->internals.mem_type = qbStructInternals_::MEM_TYPE::USER;

  return v;
}

qbVar qbString(const char* s) {  
  qbVar v{};
  v.tag = QB_TAG_STRING;
  if (s) {
    size_t len = strlen(s) + 1;
    char* str = (char*)malloc(len);
    STRCPY(str, len, s);
    v.s = str;
    v.size = len;
  }
    
  return v;
}

qbVar qbCString(char* s) {
  qbVar v{};
  v.tag = QB_TAG_CSTRING;
  v.s = s;
  v.size = s ? strlen(s) + 1 : 0;

  return v;
}

qbVar qbBytes(const char* bytes, size_t size) {
  qbVar v{};
  v.tag = QB_TAG_BYTES;
  v.size = size;

  if (bytes) {
    v.p = malloc(size);
    memcpy(v.p, bytes, size);
  } else {
    size = 0;
  }

  return v;
}

qbVar qbArray(qbTag v_type) {
  qbVar v{};
  v.tag = QB_TAG_ARRAY;
  v.size = sizeof(void*);
  v.p = new qbArray_{v_type};

  return v;
}

qbVar* qb_array_at(qbVar array, int32_t i) {
  qbArray_* a = (qbArray_*)array.p;

  if (a->array.empty()) {
    return nullptr;
  }

  i %= a->array.size();
  return &a->array[i];
}

qbVar* qb_array_append(qbVar array, qbVar v) {
  qbArray_* a = (qbArray_*)array.p;

  if (v.tag != a->type && a->type != QB_TAG_ANY) {
    return nullptr;
  }

  a->array.push_back(v);
  return &a->array.back();
}

void qb_array_insert(qbVar array, qbVar v, int32_t i) {
  qbArray_* a = (qbArray_*)array.p;

  if (v.tag != a->type && a->type != QB_TAG_ANY) {
    return;
  }

  i %= a->array.size();

  a->array.insert(a->array.begin() + i, v);
}

void qb_array_erase(qbVar array, int32_t i) {
  qbArray_* a = (qbArray_*)array.p;

  if (a->array.empty()) {
    return;
  }

  i %= a->array.size();
  a->array.erase(a->array.begin() + i);
}

void qb_array_swap(qbVar array, int32_t i, int32_t j) {
  qbArray_* a = (qbArray_*)array.p;
  if (a->array.empty()) {
    return;
  }

  std::swap(a->array[i], a->array[j]);
}

void qb_array_resize(qbVar array, size_t size) {
  qbArray_* a = (qbArray_*)array.p;
  qbVar v = {};
  v.tag = a->type;
  
  switch (v.tag) {
    case QB_TAG_NIL:
    case QB_TAG_FUTURE:
    case QB_TAG_MAP:
    case QB_TAG_ANY:
      v.size = 0;
      break;

    case QB_TAG_STRING:
      v = qbString(nullptr);
      break;

    case QB_TAG_CSTRING:
      v = qbCString(nullptr);
      break;

    case QB_TAG_BYTES:
      v = qbBytes(nullptr, 0);
      break;

    case QB_TAG_PTR:
      v.size = sizeof(void*);
      break;

    case QB_TAG_INT:
    case QB_TAG_UINT:
      v.size = sizeof(int64_t);
      break;

    case QB_TAG_DOUBLE:
      v.size = sizeof(double);
      break;

    case QB_TAG_ARRAY:
      v = qbArray(QB_TAG_ANY);
      break;
  }
  return a->array.resize(size, v);
}

size_t qb_array_count(qbVar array) {
  qbArray_* a = (qbArray_*)array.p;
  return a->array.size();
}

qbVar* qb_array_raw(qbVar array) {
  qbArray_* a = (qbArray_*)array.p;
  return a->array.data();
}

qbTag qb_array_type(qbVar array) {
  qbArray_* a = (qbArray_*)array.p;
  return a->type;
}

qbBool qb_array_iterate(qbVar array, qbBool(*it)(qbVar* k, qbVar state), qbVar state) {
  if (array.tag != QB_TAG_ARRAY) {
    return QB_FALSE;
  }

  for (auto& v : ((qbArray_*)array.p)->array) {
    if (!it(&v, state)) {
      return QB_FALSE;
    }
  }
  return QB_TRUE;
}

qbVar qbMap(qbTag k_type, qbTag v_type) {
  if (k_type != QB_TAG_PTR &&
      k_type != QB_TAG_INT &&
      k_type != QB_TAG_UINT &&
      k_type != QB_TAG_DOUBLE &&
      k_type != QB_TAG_STRING &&
      k_type != QB_TAG_CSTRING &&
      k_type != QB_TAG_ANY) {
    return qbNil;
  }

  qbVar v{};
  v.tag = QB_TAG_MAP;
  v.size = sizeof(void*);
  v.p = new qbMap_{ k_type, v_type, {} };

  return v;
}

qbVar* qb_map_at(qbVar map, qbVar k) {
  qbMap_* m = (qbMap_*)map.p;

  if (k.tag != m->k_type && m->k_type != QB_TAG_ANY) {
    return nullptr;
  }

  auto it = m->map.insert({ { k }, qbVar{} }).first;
  return &it->second;
}

qbVar* qb_map_insert(qbVar map, qbVar k, qbVar v) {
  qbVar* ret = qb_map_at(map, k);
  *ret = v;
  return ret;
}

void qb_map_erase(qbVar map, qbVar k) {
  qbMap_* m = (qbMap_*)map.p;

  if (k.tag != m->k_type) {
    return;
  }

  auto it = m->map.find({ k });
  if (it != m->map.end()) {
    m->map.erase(it);
  }
}

void qb_map_swap(qbVar map, qbVar a, qbVar b) {
  qbMap_* m = (qbMap_*)map.p;

  if (a.tag != m->k_type || b.tag != m->k_type) {
    return;
  }

  std::swap(m->map[{a}], m->map[{b}]);
}

size_t qb_map_count(qbVar map) {
  qbMap_* m = (qbMap_*)map.p;
  return m->map.size();
}

qbBool qb_map_has(qbVar map, qbVar k) {
  qbMap_* m = (qbMap_*)map.p;

  if (k.tag != m->k_type) {
    return QB_FALSE;
  }

  return m->map.count({ k }) == 1 ? QB_TRUE : QB_FALSE;
}

qbTag qb_map_keytype(qbVar map) {
  qbMap_* m = (qbMap_*)map.p;
  return m->k_type;
}

qbTag qb_map_valtype(qbVar map) {
  qbMap_* m = (qbMap_*)map.p;
  return m->v_type;
}

qbBool qb_map_iterate(qbVar map, qbBool(*it)(qbVar k, qbVar* v, qbVar state), qbVar state) {
  if (map.tag != QB_TAG_MAP) {
    return QB_FALSE;
  }

  for (auto& p : ((qbMap_*)map.p)->map) {
    if (!it(p.first.v, &p.second, state)) {
      return QB_FALSE;
    }
  }

  return QB_TRUE;
}

qbVar qb_var_at(qbVar var, qbVar key) {
  qbRef r = qb_ref_at(var, key);
  return r ? *r : qbNil;
}

qbRef qb_ref_at(qbVar var, qbVar key) {
  qbStruct_* s = (qbStruct_*)var.p;

  switch (var.tag) {
    case QB_TAG_ARRAY:
      if (key.tag == QB_TAG_INT) {
        return qb_array_at(var, (int32_t)key.i);
      } else {
        return qbRef{};
      }
    case QB_TAG_MAP:
      return qb_map_at(var, key);
    case QB_TAG_STRUCT:
      if (key.tag == QB_TAG_INT) {
        return qb_struct_ati(var, (int32_t)key.i);
      } else if (key.tag == QB_TAG_UINT) {
        return qb_struct_ati(var, (uint8_t)key.u);
      } else if (key.tag == QB_TAG_STRING || key.tag == QB_TAG_CSTRING) { 
        return qb_struct_at(var, (const char*)key.s);
      } else {
        return qbRef{};
      }
  }

  return qbRef{};
}

uint8_t qb_struct_numfields(qbVar v) {
  if (v.tag != QB_TAG_STRUCT) {
    return 0;
  }

  return (uint8_t)((qbStruct_*)v.p)->internals.schema->fields.size();
}

size_t qb_buffer_writestr(qbBuffer buf, ptrdiff_t* pos, size_t len, const char* s) {
  ptrdiff_t saved = *pos;

  size_t total = 0;
  size_t written = qb_buffer_writell(buf, pos, len);
  total += written;
  if (written == 0) {
    *pos = saved;
    return 0;
  }

  written = qb_buffer_write(buf, pos, len, s);
  total += written;
  if (written != len) {
    *pos = saved;
    return 0;
  }

  return total;
}


size_t qb_buffer_writell(qbBuffer buf, ptrdiff_t* pos, int64_t n) {
  ptrdiff_t saved = *pos;

  n = qb_htonll(n);
  size_t written = qb_buffer_write(buf, pos, 8, &n);
  if (written != 8) {
    *pos = saved;
    return 0;
  }
  return written;
}

size_t qb_buffer_writel(qbBuffer buf, ptrdiff_t* pos, int32_t n) {
  ptrdiff_t saved = *pos;

  n = qb_htonl(n);
  size_t written = qb_buffer_write(buf, pos, 4, &n);
  if (written != 4) {
    *pos = saved;
    return 0;
  }
  return written;
}

size_t qb_buffer_writes(qbBuffer buf, ptrdiff_t* pos, int16_t n) {
  ptrdiff_t saved = *pos;

  n = qb_htons(n);
  size_t written = qb_buffer_write(buf, pos, 2, &n);
  if (written != 2) {
    *pos = saved;
    return 0;
  }
  return written;
}

size_t qb_buffer_writec(qbBuffer buf, ptrdiff_t* pos, uint8_t c) {
  ptrdiff_t saved = *pos;

  size_t written = qb_buffer_write(buf, pos, 1, &c);
  if (written != 1) {
    *pos = saved;
    return 0;
  }
  return written;
}

size_t qb_buffer_writed(qbBuffer buf, ptrdiff_t* pos, double n) {
  ptrdiff_t saved = *pos;

  union {
    double d;
    int64_t i;
  } d_to_i;

  d_to_i.d = n;
  d_to_i.i = qb_htonll(d_to_i.i);
  size_t written = qb_buffer_write(buf, pos, 8, &d_to_i);
  if (written != 8) {
    *pos = saved;
    return 0;
  }
  return written;
}

size_t qb_buffer_writef(qbBuffer buf, ptrdiff_t* pos, float n) {
  ptrdiff_t saved = *pos;

  union {
    float f;
    uint32_t i;
  } f_to_i;

  f_to_i.f = n;
  f_to_i.i = qb_htonl(f_to_i.i);
  size_t written = qb_buffer_write(buf, pos, 4, &f_to_i);
  if (written != 4) {
    *pos = saved;
    return 0;
  }
  return written;
}

size_t qb_buffer_readstr(const qbBuffer_* buf, ptrdiff_t* pos, size_t* len, char** s) {
  ptrdiff_t saved = *pos;

  size_t read = qb_buffer_readll(buf, pos, (int64_t*)len);
  if (read == 0) {
    *pos = saved;
    return 0;
  }

  *s = (char*)(buf->bytes + *pos);
  *pos += *len;
  read += *len;

  return read;
}

size_t qb_buffer_readll(const qbBuffer_* buf, ptrdiff_t* pos, int64_t* n) {
  ptrdiff_t saved = *pos;

  size_t read = qb_buffer_read(buf, pos, 8, n);
  *n = qb_ntohll(*n);

  if (read != 8) {
    *pos = saved;
    return 0;
  }

  return read;
}

size_t qb_buffer_readl(const qbBuffer_* buf, ptrdiff_t* pos, int32_t* n) {
  ptrdiff_t saved = *pos;

  size_t read = qb_buffer_read(buf, pos, 4, n);
  *n = qb_ntohl(*n);

  if (read != 4) {
    *pos = saved;
    return 0;
  }

  return read;
}

size_t qb_buffer_reads(const qbBuffer_* buf, ptrdiff_t* pos, int16_t* n) {
  ptrdiff_t saved = *pos;

  size_t read = qb_buffer_read(buf, pos, 2, n);
  *n = qb_ntohs(*n);

  if (read != 2) {
    *pos = saved;
    return 0;
  }

  return read;
}

size_t qb_buffer_readc(const qbBuffer_* buf, ptrdiff_t* pos, uint8_t* c) {
  ptrdiff_t saved = *pos;

  size_t read = qb_buffer_read(buf, pos, 1, c);
  if (read != 1) {
    *pos = saved;
    return 0;
  }

  return read;
}

size_t qb_buffer_readd(const qbBuffer_* buf, ptrdiff_t* pos, double* n) {
  ptrdiff_t saved = *pos;

  union {
    double d;
    int64_t i;
  } d_to_i;

  size_t read = qb_buffer_read(buf, pos, 8, &d_to_i);

  d_to_i.i = qb_ntohll(d_to_i.i);  
  *n = d_to_i.d;

  if (read != 8) {
    *pos = saved;
    return 0;
  }

  return read;
}

size_t qb_buffer_readf(const qbBuffer_* buf, ptrdiff_t* pos, float* n) {
  ptrdiff_t saved = *pos;

  union {
    float f;
    uint32_t i;
  } f_to_i;

  size_t read = qb_buffer_read(buf, pos, 4, &f_to_i);

  f_to_i.i = qb_ntohl(f_to_i.i);
  *n = f_to_i.f;

  if (read != 4) {
    *pos = saved;
    return 0;
  }

  return read;
}

size_t qb_var_pack(qbVar v, qbBuffer_* buf, ptrdiff_t* pos) {
  ptrdiff_t saved_pos = *pos;
  size_t total = 0;
  size_t written = 0;

  uint16_t tag = v.tag & 0xFFFF;
  written = qb_buffer_write(buf, pos, 2, &tag);
  total += written;
  if (written != 2) {
    goto incomplete_write;
  }

  switch (v.tag) {
    case QB_TAG_INT:
    case QB_TAG_UINT:
    {
      written = qb_buffer_writell(buf, pos, v.i);
      total += written;
      if (written == 0) {
        goto incomplete_write;
      }
      break;
    }      

    case QB_TAG_DOUBLE:
    {
      written = qb_buffer_writed(buf, pos, v.d);
      total += written;
      if (written == 0) {
        goto incomplete_write;
      }
      break;
    }

    case QB_TAG_CSTRING:
    case QB_TAG_STRING:
      written = qb_buffer_writestr(buf, pos, v.size, v.s);
      total += written;
      if (written == 0) {
        goto incomplete_write;
      }
      break;

    case QB_TAG_BYTES:
      written = qb_buffer_writell(buf, pos, v.size);
      total += written;
      if (written == 0) {
        goto incomplete_write;
      }

      written = qb_buffer_write(buf, pos, v.size, v.p);
      total += written;
      if (written != v.size) {
        goto incomplete_write;
      }
      break;

    case QB_TAG_STRUCT:
    {
      qbSchema schema = qb_struct_getschema(v);
      const char* name = qb_schema_name(schema);
      size_t len = strlen(name) + 1;

      written = qb_buffer_writell(buf, pos, v.size);
      total += written;
      if (written == 0) {
        goto incomplete_write;
      }

      written = qb_buffer_writestr(buf, pos, len, name);
      total += written;
      if (written == 0) {
        goto incomplete_write;
      }

      uint8_t num_fields = qb_struct_numfields(v);
      for (auto i = 0; i < num_fields; ++i) {
        qbVar* var = qb_struct_ati(v, i);
        written = qb_var_pack(*var, buf, pos);
        total += written;
        if (written == 0) {
          goto incomplete_write;
        }
      }
      
      break;
    }

    case QB_TAG_ARRAY:
    {
      written = qb_buffer_writell(buf, pos, v.size);
      total += written;
      if (written == 0) {
        goto incomplete_write;
      }

      written = qb_buffer_writel(buf, pos, qb_array_type(v));
      total += written;
      if (written == 0) {
        goto incomplete_write;
      }

      size_t count = qb_array_count(v);
      written = qb_buffer_writell(buf, pos, count);
      total += written;
      if (written == 0) {
        goto incomplete_write;
      }

      for (size_t i = 0; i < count; ++i) {
        qbVar* var = qb_array_at(v, (int32_t)i);
        written = qb_var_pack(*var, buf, pos);
        total += written;
        if (written == 0) {
          goto incomplete_write;
        }
      }

      break;
    }

    case QB_TAG_MAP:
    {
      written = qb_buffer_writell(buf, pos, v.size);
      total += written;
      if (written == 0) {
        goto incomplete_write;
      }

      written = qb_buffer_writel(buf, pos, qb_map_keytype(v));
      total += written;
      if (written == 0) {
        goto incomplete_write;
      }

      written = qb_buffer_writel(buf, pos, qb_map_valtype(v));
      total += written;
      if (written == 0) {
        goto incomplete_write;
      }

      struct StreamState {
        qbBuffer_* buf;
        ptrdiff_t* pos;
        size_t* total;
      } stream_state { buf, pos, &total };

      size_t count = qb_map_count(v);
      written = qb_buffer_writell(buf, pos, count);
      total += written;
      if (written == 0) {
        goto incomplete_write;
      }

      if (!qb_map_iterate(v, [](qbVar k, qbVar* v, qbVar state) {
        StreamState* s = (StreamState*)state.p;

        size_t written = qb_var_pack(k, s->buf, s->pos);
        s->total += written;
        if (written == 0) {
          return QB_FALSE;
        }

        written = qb_var_pack(*v, s->buf, s->pos);
        s->total += written;
        if (written == 0) {
          return QB_FALSE;
        }

        return written == 0 ? QB_FALSE : QB_TRUE;
      }, qbPtr(&stream_state))) {
        goto incomplete_write;
      }
      break;
    }
  }

  return total;

incomplete_write:
  *pos = saved_pos;
  return 0;
}

size_t qb_var_unpack(qbVar* v, const qbBuffer_* buf, ptrdiff_t* pos) {
  ptrdiff_t saved_pos = *pos;
  size_t total = 0;
  size_t bytes_read = 0;

  v->tag = QB_TAG_NIL;
  bytes_read = qb_buffer_read(buf, pos, 2, &v->tag);
  total += bytes_read;
  if (bytes_read != 2) {
    goto incomplete_read;
  }

  switch (v->tag) {
    case QB_TAG_INT:
    case QB_TAG_UINT:
    {
      bytes_read = qb_buffer_readll(buf, pos, &v->i);
      total += bytes_read;
      if (bytes_read == 0) {
        goto incomplete_read;
      }
      break;
    }

    case QB_TAG_DOUBLE:
    {
      bytes_read = qb_buffer_readd(buf, pos, &v->d);
      total += bytes_read;
      if (bytes_read == 0) {
        goto incomplete_read;
      }
      break;
    }

    case QB_TAG_CSTRING:
    case QB_TAG_STRING:      
      bytes_read = qb_buffer_readstr(buf, pos, &v->size, &v->s);
      qbVar str = qbString(nullptr);
      str.size = v->size;
      str.s = (char*)malloc(v->size);
      memcpy(str.s, v->s, str.size);
      *v = str;

      total += bytes_read;
      if (bytes_read == 0) {
        goto incomplete_read;
      }
      break;

    case QB_TAG_BYTES:
      bytes_read = qb_buffer_readll(buf, pos, (int64_t*)&v->size);
      total += bytes_read;
      if (bytes_read == 0) {
        goto incomplete_read;
      }

      v->p = malloc(v->size);
      bytes_read = qb_buffer_read(buf, pos, v->size, v->p);
      total += bytes_read;
      if (bytes_read != v->size) {
        goto incomplete_read;
      }
      break;

    case QB_TAG_STRUCT:
    {      
      char* name;
      size_t len;

      bytes_read = qb_buffer_readll(buf, pos, (int64_t*)&v->size);
      total += bytes_read;
      if (bytes_read == 0) {
        goto incomplete_read;
      }

      bytes_read = qb_buffer_readstr(buf, pos, &len, &name);
      total += bytes_read;
      if (bytes_read == 0) {
        goto incomplete_read;
      }

      qbSchema schema = qb_schema_find(name);
      if (!schema) {
        goto incomplete_read;
      }

      *v = qb_struct_create(schema, nullptr);

      {
        uint8_t num_fields = qb_schema_numfields(schema);

        for (auto i = 0; i < num_fields; ++i) {
          qbVar* val = qb_struct_ati(*v, i);
          bytes_read = qb_var_unpack(val, buf, pos);
          total += bytes_read;
          if (bytes_read == 0) {
            goto incomplete_read;
          }
        }
      }

      break;
    }

    case QB_TAG_ARRAY:
    {
      bytes_read = qb_buffer_readll(buf, pos, (int64_t*)&v->size);
      total += bytes_read;
      if (bytes_read == 0) {
        goto incomplete_read;
      }

      qbTag array_type;
      bytes_read = qb_buffer_readl(buf, pos, (int32_t*)&array_type);
      total += bytes_read;
      if (bytes_read == 0) {
        goto incomplete_read;
      }

      *v = qbArray(array_type);

      size_t count;
      bytes_read = qb_buffer_readl(buf, pos, (int32_t*)&count);
      total += bytes_read;
      if (bytes_read == 0) {
        goto incomplete_read;
      }

      for (size_t i = 0; i < count; ++i) {
        qbVar var;
        bytes_read = qb_var_unpack(&var, buf, pos);
        total += bytes_read;
        if (bytes_read == 0) {
          goto incomplete_read;
        }

        qb_array_append(*v, var);
      }

      break;
    }

    case QB_TAG_MAP:
    {
      bytes_read = qb_buffer_readll(buf, pos, (int64_t*)&v->size);
      total += bytes_read;
      if (bytes_read == 0) {
        goto incomplete_read;
      }

      qbTag key_type;
      bytes_read = qb_buffer_readl(buf, pos, (int32_t*)&key_type);
      total += bytes_read;
      if (bytes_read == 0) {
        goto incomplete_read;
      }

      qbTag val_type;
      bytes_read = qb_buffer_readl(buf, pos, (int32_t*)&val_type);
      total += bytes_read;
      if (bytes_read == 0) {
        goto incomplete_read;
      }
      *v = qbMap(key_type, val_type);

      size_t count;
      bytes_read = qb_buffer_readll(buf, pos, (int64_t*)&count);
      total += bytes_read;
      if (bytes_read == 0) {
        goto incomplete_read;
      }

      for (size_t i = 0; i < count; ++i) {
        qbVar key;
        qbVar val;

        size_t bytes_read = qb_var_unpack(&key, buf, pos);
        total += bytes_read;
        if (bytes_read == 0) {
          goto incomplete_read;
        }

        bytes_read = qb_var_unpack(&val, buf, pos);
        total += bytes_read;
        if (bytes_read == 0) {
          goto incomplete_read;
        }

        qb_map_insert(*v, key, val);
      }
      break;
    }
  }

  return total;

incomplete_read:
  *pos = saved_pos;
  return 0;
}

void qb_var_copy(const qbVar* from, qbVar* to) {
  if (from == to) {
    return;
  }

  qb_var_destroy(to);

  if (from->tag == QB_TAG_STRING) {
    *to = qbString(from->s);
  } else if (from->tag == QB_TAG_BYTES) {
    *to = qbBytes((const char*)from->p, from->size);
  } else {
    memcpy(to, from, sizeof(qbVar));
  }
}

void qb_var_move(qbVar* from, qbVar* to) {
  if (from == to) {
    return;
  }

  qb_var_destroy(to);

  memcpy(to, from, sizeof(qbVar));
  *from = {};
}

void qb_var_destroy(qbVar* v) {
  switch (v->tag) {
    case QB_TAG_STRING:
      free(v->s);
      break;

    case QB_TAG_BYTES:
      free(v->p);
      break;

    case QB_TAG_ARRAY:
    {
      qbArray_* a = (qbArray_*)v->p;
      for (auto& var : a->array) {
        qb_var_destroy(&var);
      }
      delete a;
      break;
    }

    case QB_TAG_MAP:
      delete (qbMap_*)v->p;
      break;

    case QB_TAG_STRUCT:
      qb_struct_destroy(v);
      break;
  }

  *v = {};
}

qbVar qb_var_tostring(qbVar v) {
  qbVar s;

  std::string str;
  switch (v.tag) {
    case QB_TAG_NIL:
      str = "[NIL]";
      break;

    case QB_TAG_FUTURE:
      str = "[FUTURE]";
      break;

    case QB_TAG_ANY:
      str = "[ANY]";
      break;

    case QB_TAG_PTR:
    {
      char hex_string[sizeof(v.i) * 2 + 2];

      sprintf_s(hex_string, sizeof(hex_string), "%08llX", v.i);
      str = std::string("[PTR]: 0x") + hex_string;
      break;
    }

    case QB_TAG_INT:
      str = std::string("[INT]: ") + std::to_string(v.i);
      break;

    case QB_TAG_UINT:
      str = std::string("[UINT]: ") + std::to_string(v.u);
      break;

    case QB_TAG_DOUBLE:
      str = std::string("[DOUBLE]: ") + std::to_string(v.d);
      break;

    case QB_TAG_STRING:
      str = std::string("[STRING]: ") + v.s;
      break;

    case QB_TAG_CSTRING:
      str = std::string("[CSTRING]: ") + v.s;
      break;

    case QB_TAG_STRUCT:
    {
      str = std::string("[STRUCT]: { ");
      qbSchema schema = qb_struct_getschema(v);

      for (auto i = 0; i < qb_struct_numfields(v); ++i) {
        qbVar val = qb_var_tostring(*qb_struct_ati(v, i));
        if (i != 0) {
          str += ", ";
        }
        str += std::string("(") + qb_struct_keyat(v, i) + ", " + val.s + ")";

        qb_var_destroy(&val);
      }
      str += " }";
      break;
    }

    case QB_TAG_ARRAY:
    {
      str = std::string("[ARRAY]: [ ");

      for (auto i = 0; i < qb_array_count(v); ++i) {
        if (i != 0) {
          str += ", ";
        }
        qbVar val = qb_var_tostring(*qb_array_at(v, i));
        str += val.s;

        qb_var_destroy(&val);
      }
      str += " ]";
      break;
    }

    case QB_TAG_MAP:
    {
      str = std::string("[MAP]: { ");

      struct stream_state {
        bool first;
        std::string& str;
      } state{ true, str };

      qb_map_iterate(v, [](qbVar k, qbVar* v, qbVar state_var) {
        struct stream_state* state = (struct stream_state*)state_var.p;
        
        if (!state->first) {
          state->str += ", ";
        }        

        qbVar k_str = qb_var_tostring(k);
        qbVar v_str = qb_var_tostring(*v);
        state->str += std::string("(") + k_str.s + ", " + v_str.s + ")";

        state->first = false;
        qb_var_destroy(&k_str);
        qb_var_destroy(&v_str);
        
        return QB_TRUE;
      }, qbPtr(&state));

      str += " }";
      break;
    }
  }

  if (!str.empty()) {
    s = qbString(str.data());
  }

  return s;
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

qbResult qb_scene_save(qbScene* scene, const utf8_t* file) {
  return QB_OK;
}

qbResult qb_scene_load(qbScene* scene, const char* name, const utf8_t* file) {
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

const char* qb_schema_name(qbSchema schema) {
  return schema->name.data();
}

qbComponent qb_schema_component(qbSchema schema) {
  return schema->component;
}

size_t qb_schema_size(qbSchema schema) {
  return schema->size;
}

size_t qb_struct_size(qbSchema schema) {
  return sizeof(qbStructInternals_) + schema->size;
}

uint8_t qb_schema_fields(qbSchema schema, qbSchemaField* fields) {  
  if (schema->fields.size() > 255) {
    *fields = nullptr;
    return 0;
  }

  *fields = schema->fields.data();
  return (uint8_t)schema->fields.size();
}

uint8_t qb_schema_numfields(qbSchema schema) {
  if (schema->fields.size() > 255) {
    return 0;
  }
  return (uint8_t)schema->fields.size();
}

qbSchema qb_struct_getschema(qbVar v) {
  if (v.tag != QB_TAG_STRUCT) {
    return nullptr;
  }
  return ((qbStruct_*)v.p)->internals.schema;
}

const char* qb_schemafield_name(qbSchemaField field) {
  return field->key.c_str();
}

qbTag qb_schemafield_tag(qbSchemaField field) {
  return field->type.tag;
}

size_t qb_schemafield_offset(qbSchemaField field) {
  return field->offset;
}

size_t qb_schemafield_size(qbSchemaField field) {
  return field->size;
}

size_t qb_buffer_write(qbBuffer buf, ptrdiff_t* pos, size_t size, const void* bytes) {
  if (!buf || !pos || !bytes || !size) {
    return 0;
  }

  size = std::min(size, buf->capacity);
  *pos = std::min(std::max(0LL, *pos), (ptrdiff_t)buf->capacity);
  ptrdiff_t end = std::min(*pos + size, buf->capacity);
  ptrdiff_t write_size = std::max(end - *pos, 0LL);

  memcpy(buf->bytes + *pos, bytes, write_size);

  *pos += write_size;
  return write_size;
}

size_t qb_buffer_read(const qbBuffer_* buf, ptrdiff_t* pos, size_t size, void* bytes) {
  if (!buf || !pos || !bytes || !size) {
    return 0;
  }

  size = std::min(size, buf->capacity);
  *pos = std::min(std::max(0LL, *pos), (ptrdiff_t)buf->capacity);
  ptrdiff_t end = std::min(*pos + size, buf->capacity);
  ptrdiff_t read_size = std::max(end - *pos, 0LL);

  memcpy(bytes, buf->bytes + *pos, read_size);

  *pos += read_size;
  return read_size;
}

qbComponent qb_uid() {
  return qb_id_component;
}