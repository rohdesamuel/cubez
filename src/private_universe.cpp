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

#include "private_universe.h"
#include "system_impl.h"
#include "snapshot.h"
#include "query.h"
#include "entity_table.h"

#include <stdarg.h>

#ifdef __COMPILE_AS_WINDOWS__
#undef CreateEvent
#undef SendMessage
#endif

typedef Runner::State RunState;

thread_local qbId PrivateUniverse::program_id;
thread_local qbScene PrivateUniverse::working_scene = nullptr;

extern qbUniverse* universe_;

void Runner::wait_until(const std::vector<State>& allowed) {
  while (std::find(allowed.begin(), allowed.end(), state_) == allowed.end());
}

void Runner::wait_until(std::vector<State>&& allowed) {
  while (std::find(allowed.begin(), allowed.end(), state_) == allowed.end());
}

qbResult Runner::transition(
    State allowed, State next) {
  return transition(std::vector<State>{allowed}, next);
}

#ifdef __ENGINE_DEBUG__
qbResult Runner::transition(const std::vector<State>& allowed, State next) {
  std::lock_guard<std::mutex> lock(state_change_);
  if (assert_in_state(allowed) == QB_OK) {
    state_ = next;
    return QB_OK;
  }
  state_ = State::ERROR;
  return QB_ERROR_BAD_RUN_STATE;
}

qbResult Runner::transition(
    std::vector<State>&& allowed, State next) {
  std::lock_guard<std::mutex> lock(state_change_);
  if (assert_in_state(std::move(allowed)) == QB_OK) {
    state_ = next;
    return QB_OK;
  }
  state_ = State::ERROR;
  return QB_ERROR_BAD_RUN_STATE;
}
#else
qbResult Runner::transition(const std::vector<State>&, State next) {
  //std::lock_guard<std::mutex> lock(state_change_);
  state_ = next;
  return QB_OK;
}
qbResult Runner::transition(std::vector<State>&&, State next) {
  //std::lock_guard<std::mutex> lock(state_change_);
  state_ = next;
  return QB_OK;
}
#endif

qbResult Runner::assert_in_state(State allowed) {
  return assert_in_state(std::vector<State>{allowed});
}

qbResult Runner::assert_in_state(const std::vector<State>& allowed) {
  if (std::find(allowed.begin(), allowed.end(), state_) != allowed.end()) {
    return QB_OK;
  }
  DEBUG_OP(
      std::string allowed_states = "";
      for (auto state : allowed) {
        allowed_states += state_to_string(state) + "\n";
      }
  );
  
  return QB_ERROR_BAD_RUN_STATE;
}

qbResult Runner::assert_in_state(std::vector<State>&& allowed) {
  if (std::find(allowed.begin(), allowed.end(), state_) != allowed.end()) {
    return QB_OK;
  }
  DEBUG_OP(
    std::string allowed_states = "";
    for (auto state : allowed) {
      allowed_states += state_to_string(state) + "\n";
    }
  );
  
  return QB_ERROR_BAD_RUN_STATE;
}

PrivateUniverse::PrivateUniverse() {
  programs_ = std::make_unique<ProgramRegistry>();
  components_ = std::make_unique<ComponentRegistry>();

  scene_create(&working_scene, "");
  baseline_ = active_ = working_scene;

  // Create the default program.
  create_program("");
}

PrivateUniverse::~PrivateUniverse() {}

qbResult PrivateUniverse::init() {
  return runner_.transition(RunState::STOPPED, RunState::INITIALIZED);
}

qbResult PrivateUniverse::start() {
  lua_start(programs_->main_lua_state());
  return runner_.transition(RunState::INITIALIZED, RunState::STARTED);
}

qbResult PrivateUniverse::loop() {
  // Reset the working scene to the active scene.
  scene_reset();
  runner_.transition({RunState::RUNNING, RunState::STARTED}, RunState::LOOPING);

  ActiveScene()->Flush();
  programs_->Run(ActiveScene());

  return runner_.transition(RunState::LOOPING, RunState::RUNNING);
}

qbResult PrivateUniverse::stop() {
  return runner_.transition({RunState::RUNNING, RunState::UNKNOWN}, RunState::STOPPED);
}

qbId PrivateUniverse::create_program(const char* name) {
  return programs_->CreateProgram(name);
}

qbResult PrivateUniverse::run_program(qbId program) {
  return programs_->RunProgram(program, ActiveScene());
}

qbResult PrivateUniverse::detach_program(qbId program) {
  return programs_->DetatchProgram(program, [this]() { return ActiveScene(); });
}

qbResult PrivateUniverse::join_program(qbId program) {
  return programs_->JoinProgram(program);
}

lua_State* PrivateUniverse::main_lua_state() {
  return programs_->main_lua_state();
}

void PrivateUniverse::onready_program(
  qbId program, void(*onready)(qbProgram* program, qbVar), qbVar state) {
  qbProgram* p = programs_->GetProgram(program);
  ProgramImpl::FromRaw(p)->SubscribeToOnReady(onready, state);
}

qbResult PrivateUniverse::system_create(qbSystem* system, 
                                        const qbSystemAttr_& attr) {
  qbProgram* p = programs_->GetProgram(attr.program);
  if (!p) {
    return qbResult::QB_UNKNOWN;
  }

  *system = ProgramImpl::FromRaw(p)->CreateSystem(attr);

  return qbResult::QB_OK;
}

qbResult PrivateUniverse::free_system(qbSystem system) {
  ASSERT_NOT_NULL(system);

  qbProgram* p = programs_->GetProgram(system->program);
  ASSERT_NOT_NULL(p);

  return ProgramImpl::FromRaw(p)->FreeSystem(system);
}

qbSystem PrivateUniverse::copy_system(
    qbSystem, const char*) {
  return nullptr;
}

qbResult PrivateUniverse::enable_system(qbSystem system) {
  ASSERT_NOT_NULL(system);

  qbProgram* p = programs_->GetProgram(system->program);
  ASSERT_NOT_NULL(p);

  return ProgramImpl::FromRaw(p)->EnableSystem(system);
}

qbResult PrivateUniverse::disable_system(qbSystem system) {
  ASSERT_NOT_NULL(system);

  qbProgram* p = programs_->GetProgram(system->program);
  ASSERT_NOT_NULL(p);

  return ProgramImpl::FromRaw(p)->DisableSystem(system);
}

qbVar PrivateUniverse::run_system(qbSystem system, qbVar arg) {
  auto s = SystemImpl::FromRaw(system);
  return s->Run(ActiveScene(), nullptr, arg);
}

qbResult PrivateUniverse::foreach_system(qbComponent* components, size_t component_count,
                                         qbVar var, void(*fn)(qbInstance, qbVar)) {
  qbSystemAttr_ attr = {};

  qb_systemattr_setjoin(&attr, QB_JOIN_LEFT);

  std::vector<qbComponent> components_v;
  components_v.reserve(component_count);

  for (size_t i = 0; i < component_count; ++i) {
    qb_systemattr_addmutable(&attr, components[i]);
    components_v.push_back(components[i]);
  }

  struct ForEachState {
    void(*fn)(qbInstance, qbVar);
    qbVar var;
  };

  ForEachState state{ fn, var };

  qb_systemattr_setuserstate(&attr, &state);
  qb_systemattr_setfunction(&attr, [](qbInstance inst, qbFrame* frame) {
    ForEachState* state = (ForEachState*)frame->state;
    state->fn(inst, state->var);
  });

  SystemImpl s(attr, nullptr, components_v);
  s.Run(ActiveScene());

  return QB_OK;
}

qbResult PrivateUniverse::do_query(qbQuery query, qbVar arg) {
  return Query(query, ActiveScene())(arg);
}

void PrivateUniverse::component_iterate(qbComponent component, qbIteratorImpl_* impl, va_list components) {
  *impl = qbIteratorImpl_{};
  impl->components[0] = WorkingScene()->ComponentGet(component);
  impl->num_components = 1;

  qbComponent to_join = va_arg(components, qbComponent);
  for (size_t i = 1; i < QB_MAX_ITERATOR_COMPONENT_COUNT && to_join != qbInvalidComponent; ++i) {
    Component* component = WorkingScene()->ComponentGet(to_join);
    impl->components[impl->num_components] = component;
    ++impl->num_components;

    to_join = va_arg(components, qbComponent);
  }
}

void PrivateUniverse::component_iterate(qbComponent component, qbIteratorImpl_* impl,
  size_t count, qbComponent components[]) {

  *impl = qbIteratorImpl_{};
  impl->components[0] = WorkingScene()->ComponentGet(component);
  impl->num_components = 1;

  for (size_t i = 0; i < QB_MAX_ITERATOR_COMPONENT_COUNT && i < count; ++i){
    Component* component = WorkingScene()->ComponentGet(components[i]);
    impl->components[impl->num_components++] = component;
  }
}

qbBool PrivateUniverse::iterator_next(qbIterator it) {
  qbIteratorImpl_* impl = (qbIteratorImpl_*)it;
  if (impl->num_components == 0) {
    return QB_FALSE;
  }

  Component* component = impl->components[0];

  Component::iterator c_it = component->begin() + impl->index;
  Component::iterator c_end = component->end();
  while (c_it != c_end) {
    auto [id, data] = *c_it;
    
    bool has_entity = true;
    for (size_t i = 1; i < impl->num_components; ++i) {
      Component* c = impl->components[i];
      if (!c->Has(id)) {
        has_entity = false;
        break;
      }
    }
    ++c_it;
    ++impl->index;

    if (has_entity) {
      return QB_TRUE;
    }
  }

  return QB_FALSE;
}

void PrivateUniverse::iterator_get(qbIterator it, va_list args) {
  qbIteratorImpl_* impl = (qbIteratorImpl_*)it;
  
  DEBUG_ASSERT(impl->index > 0, 1);
  Component* component = impl->components[0];
  Component::iterator c_it = component->begin() + (impl->index - 1);
  DEBUG_ASSERT(c_it != component->end(), 1);

  auto [entity, pbuf] = *c_it;

  uintptr_t p  = va_arg(args, uintptr_t);
  if (p) {
    *(void**)p = pbuf;
  }

  for (size_t i = 0; i < impl->num_components && p != 0xCD; ++i) {
    if (p) {
      Component* c = impl->components[i];
      *(void**)p = c->at(entity);
    }
    p = va_arg(args, uintptr_t);
  }
}

void PrivateUniverse::iterator_get(qbIterator it, size_t count, void* pbufs[]) {
  qbIteratorImpl_* impl = (qbIteratorImpl_*)it;

  DEBUG_ASSERT(impl->index > 0, 1);
  Component* component = impl->components[0];
  Component::iterator c_it = component->begin() + (impl->index - 1);
  DEBUG_ASSERT(c_it != component->end(), 1);

  auto [entity, pbuf] = *c_it;
  for (size_t i = 0; i < impl->num_components; ++i) {
    void* p = pbufs[i];
    if (p) {
      Component* c = impl->components[i];
      *(void**)p = c->at(entity);
    }
  }
}

qbBool PrivateUniverse::iterator_component(qbIterator it, qbComponent component, void* pbuf) {
  qbIteratorImpl_* impl = (qbIteratorImpl_*)it;

  DEBUG_ASSERT(impl->index != 0, 1);
  Component* comp = impl->components[0];
  Component::iterator c_it = comp->begin() + (impl->index - 1);
  DEBUG_ASSERT(c_it != comp->end(), 1);

  auto [entity, _] = *c_it;

  for (size_t i = 0; i < impl->num_components; ++i) {
    Component* c = impl->components[i];
    if (c->Id() == component) {
      *(void**)pbuf = (*c)[entity];
      return QB_TRUE;
    }
  }

  return QB_FALSE;
}

void PrivateUniverse::iterator_index(qbIterator it, size_t index, void* pbuf) {
  qbIteratorImpl_* impl = (qbIteratorImpl_*)it;

  DEBUG_ASSERT(impl->index > 0, 1);
  Component* component = impl->components[0];
  Component::iterator c_it = component->begin() + (impl->index - 1);
  DEBUG_ASSERT(c_it != component->end(), 1);
  DEBUG_ASSERT(index < impl->num_components, 1);

  auto [entity, d] = *c_it;
  *(void**)pbuf = d;
}

qbEntity PrivateUniverse::iterator_entity(qbIterator it) {
  qbIteratorImpl_* impl = (qbIteratorImpl_*)it;
  DEBUG_ASSERT(impl->index > 0, 1);
  Component* component = impl->components[0];
  Component::iterator c_it = component->begin() + (impl->index - 1);
  DEBUG_ASSERT(c_it != component->end(), 1);

  auto [entity, _] = *c_it;
  if (impl->table_id) {
    entity = SET_ENTITY_TABLE_ID(impl->table_id, entity);
  }

  return entity;
}

qbResult PrivateUniverse::table_create(qbEntityTable* table, qbEntityTableAttr attr) {
  return WorkingScene()->TableCreate(table, attr);
}

qbResult PrivateUniverse::table_destroy(qbEntityTable* table) {
  return WorkingScene()->TableDestroy(table);
}

void PrivateUniverse::table_iterate(qbEntityTable table, qbIteratorImpl_* impl, va_list components) {
  *impl = qbIteratorImpl_{};
  qbComponent to_join = va_arg(components, qbComponent);
  if (to_join == qbInvalidComponent) {
    return;
  }

  EntityTable* entity_table = EntityTable::FromRaw(table);
  impl->table_id = entity_table->Id();

  for (size_t i = 0; i < QB_MAX_ITERATOR_COMPONENT_COUNT && to_join != qbInvalidComponent; ++i) {
    Component* component = nullptr;
    if (entity_table->has_component(to_join)) {
      component = entity_table->component(to_join);
    } else {
      component = WorkingScene()->ComponentGet(to_join);
    }
    impl->components[impl->num_components] = component;
    ++impl->num_components;

    to_join = va_arg(components, qbComponent);
  }
}

void PrivateUniverse::table_iterate(qbEntityTable table, qbIteratorImpl_* impl, size_t count, qbComponent components[]) {
  *impl = qbIteratorImpl_{};
  impl->num_components = count;

  EntityTable* entity_table = EntityTable::FromRaw(table);
  impl->table_id = entity_table->Id();

  for (size_t i = 0; i < count; ++i) {
    qbComponent to_join = components[i];
    Component* component = nullptr;
    if (entity_table->has_component(to_join)) {
      component = entity_table->component(to_join);
    } else {
      component = WorkingScene()->ComponentGet(to_join);
    }
    impl->components[i] = component;
  }
}

qbResult PrivateUniverse::event_create(qbEvent* event, qbEventAttr attr) {
  return WorkingScene()->CreateEvent(event, attr);
}

qbResult PrivateUniverse::event_destroy(qbEvent*) {
	return qbResult::QB_OK;
}

qbResult PrivateUniverse::event_subscribe(qbEvent event, qbEventFn fn, qbVar arg) {
  WorkingScene()->SubscribeTo(event, fn, arg);
	return qbResult::QB_OK;
}

qbResult PrivateUniverse::event_unsubscribe(qbEvent event, qbEventFn fn) {
  WorkingScene()->UnsubscribeFrom(event, fn);
	return qbResult::QB_OK;
}

qbResult PrivateUniverse::event_send(qbEvent event, void* message) {
  return ((Event*)event->event)->SendMessage(message);
}

qbResult PrivateUniverse::event_sendsync(qbEvent event, void* message) {
  return ((Event*)event->event)->SendMessageSync(message, ActiveScene());
}

qbResult PrivateUniverse::entity_create(qbEntity* entity, const qbEntityAttr_& attr) {
  return WorkingScene()->EntityCreate(entity, attr);
}

qbResult PrivateUniverse::entity_create(qbEntity* entity, size_t count, const qbComponentData_ data[]) {
  return WorkingScene()->EntityCreate(entity, count, data);
}

qbResult PrivateUniverse::entity_destroy(qbEntity entity) {
  return WorkingScene()->EntityDestroy(entity);
}

bool PrivateUniverse::entity_hascomponent(qbEntity entity,
                                          qbComponent component) {
  return WorkingScene()->EntityHasComponent(entity, component);
}

void* PrivateUniverse::entity_getcomponent(qbEntity entity, qbComponent component) {
  return WorkingScene()->EntityGetComponent(entity, component);
}

qbResult PrivateUniverse::entity_addcomponent(qbEntity entity,
                                              qbComponent component,
                                              void* instance_data) {
  return WorkingScene()->EntityAddComponent(entity, component, instance_data);
}

qbResult PrivateUniverse::entity_addcomponents(qbEntity entity, size_t count, const qbComponentData_ data[]) {
  return WorkingScene()->EntityAddComponents(entity, count, data);
}

qbResult PrivateUniverse::entity_removecomponent(qbEntity entity,
                                                qbComponent component) {
  return WorkingScene()->EntityRemoveComponent(entity, component);
}

qbResult PrivateUniverse::component_create(qbComponent* component, qbComponentAttr attr) {
  qbResult res = components_->Create(component, attr);
  if (res == QB_OK && attr->schema) {
    components_->RegisterSchema(*component, attr->schema);
  }

  return res;
}

size_t PrivateUniverse::component_getcount(qbComponent component) {
  return WorkingScene()->ComponentGetCount(component);
}

qbComponent PrivateUniverse::component_find(const char* name) {
  return components_->Find(name);
}

qbSchema PrivateUniverse::component_schema(qbComponent component) {
  return components_->FindSchema(component);
}

qbResult PrivateUniverse::component_oncreate(qbComponent component, qbEventFn fn, qbVar arg) {
  WorkingScene()->ComponentSubscribeToOnCreate(fn, arg, component);
  return QB_OK;
}

qbResult PrivateUniverse::component_ondestroy(qbComponent component, qbEventFn fn, qbVar arg) {
  WorkingScene()->ComponentSubscribeToOnDestroy(fn, arg, component);
  return QB_OK;
}

size_t PrivateUniverse::component_pack(qbComponent component, const qbBuffer_* read,
                                       qbBuffer_* write, ptrdiff_t* pos) {
  return WorkingScene()->ComponentGet(component)->Pack(read, write, pos);
}

size_t PrivateUniverse::component_unpack(qbComponent component, const qbBuffer_* read,
                                         qbBuffer_* write, ptrdiff_t* pos) {
  return WorkingScene()->ComponentGet(component)->Unpack(read, write, pos);
}

qbSchema PrivateUniverse::schema_find(const char* name) {
  return components_->FindSchema(name);
}

qbResult PrivateUniverse::instance_oncreate(qbComponent component,
                                            qbInstanceOnCreate on_create,
                                            qbVar state) {
  struct qbInstanceOnCreateState {
    qbInstanceOnCreate on_create;
    qbVar state;
  };
  
  qbInstanceOnCreateState* fn_state = new qbInstanceOnCreateState();
  fn_state->on_create = on_create;
  fn_state->state = state;

  WorkingScene()->ComponentSubscribeToOnCreate([](void* event_msg, qbVar arg) {
    qbInstanceOnCreateEvent_* event = (qbInstanceOnCreateEvent_*)event_msg;
    qbInstanceOnCreateState* fn_state = (qbInstanceOnCreateState*)arg.p;
    qbInstance_ instance = event->component->FindInstance(event->entity);

    fn_state->on_create(&instance, fn_state->state);
  }, qbPtr(fn_state), component);

  return QB_OK;
}

qbResult PrivateUniverse::instance_ondestroy(qbComponent component,
                                             qbInstanceOnDestroy on_destroy,
                                             qbVar state) {
  struct qbInstanceOnDestroyState {
    qbInstanceOnDestroy on_destroy;
    qbVar state;
  };

  qbInstanceOnDestroyState* fn_state = new qbInstanceOnDestroyState();
  fn_state->on_destroy = on_destroy;
  fn_state->state = state;

  WorkingScene()->ComponentSubscribeToOnDestroy([](void* event_msg, qbVar arg) {
    qbInstanceOnDestroyEvent_* event = (qbInstanceOnDestroyEvent_*)event_msg;
    qbInstanceOnDestroyState* fn_state = (qbInstanceOnDestroyState*)arg.p;
    qbInstance_ instance = event->component->FindInstance(event->entity);

    fn_state->on_destroy(&instance, fn_state->state);
  }, qbPtr(fn_state), component);

  return QB_OK;
}

qbResult PrivateUniverse::instance_getcomponent(qbInstance instance,
                                                qbComponent component,
                                                void* pbuffer) {
  *(void**)pbuffer = WorkingScene()->ComponentGetEntityData(component, instance->entity);
  return QB_OK;
}

void PrivateUniverse::instance_get(qbInstance instance, va_list args) {
  qbSystem system = instance->system;
  if (!system) {
    return;
  }

  auto s = SystemImpl::FromRaw(system);
  s->InstanceGet(WorkingScene(), instance, args);
}

void PrivateUniverse::instance_geti(qbInstance instance, size_t index, void* pbuf) {
  qbSystem system = instance->system;
  if (!system) {
    return;
  }

  auto s = SystemImpl::FromRaw(system);
  s->InstanceGeti(WorkingScene(), instance, index, pbuf);
}

void PrivateUniverse::instance_getn(qbInstance instance, size_t count, void* pbufs[]) {
  qbSystem system = instance->system;
  if (!system) {
    return;
  }

  auto s = SystemImpl::FromRaw(system);
  s->InstanceGetn(WorkingScene(), instance, count, pbufs);
}

bool PrivateUniverse::instance_hascomponent(qbInstance instance, qbComponent component) {
  return WorkingScene()->EntityHasComponent(instance->entity, component);
}

qbResult PrivateUniverse::instance_find(qbComponent component, qbEntity entity, void* pbuffer) {
  *(void**)pbuffer = WorkingScene()->ComponentGetEntityData(component, entity);
  return QB_OK;
}

qbBarrier PrivateUniverse::barrier_create() {
  qbBarrier barrier = new qbBarrier_();
  barrier->impl = new Barrier();
  return barrier;
}

void PrivateUniverse::barrier_destroy(qbBarrier barrier) {
  delete (Barrier*)(barrier)->impl;
  delete barrier;
}

qbResult PrivateUniverse::scene_create(qbScene* scene, const char* name) {
  if (scene_global() && *scene == scene_global()) {
    return QB_OK;
  }

  qbScene ret = new qbScene_();
  TableRegistry* table_registry;
  {
    auto entities = std::make_unique<EntityRegistry>();
    auto tables = std::make_unique<TableRegistry>(components_.get());
    auto instances = std::make_unique<InstanceRegistry>(*components_, *tables);
    auto events = std::make_unique<EventRegistry>();
    table_registry = tables.get();
    ret->state = new GameState(
      std::move(entities), std::move(instances), components_.get(),
      std::move(events), std::move(tables));
  }

  if (name) {
    const size_t kNameBufLen = 128;
    const size_t kMaxNameLen = kNameBufLen - 1;

    char* new_name = new char[kNameBufLen];
    memset(new_name, 0, kNameBufLen);

    size_t len = std::min(strlen(name), kMaxNameLen);
    memcpy(new_name, name, len);
    
    ret->name = new_name;
  } else {
    ret->name = new char('\0');
  }
  *scene = ret;
  
  table_registry->Init();

  return QB_OK;
}

qbResult PrivateUniverse::scene_destroy(qbScene* scene) {
  DEBUG_OP(runner_.assert_in_state({ RunState::RUNNING, RunState::STARTED }));
  if (*scene == scene_global()) {
    return QB_OK;
  }

  // Inform users of destruction.
  for (auto& fn : (*scene)->ondestroy) {
    fn(*scene,
       (*scene)->keys.empty() ? 0 : (*scene)->keys.size(),
       (*scene)->keys.empty() ? nullptr : (*scene)->keys.data(),
       (*scene)->values.empty() ? nullptr : (*scene)->values.data());
  }

  // Delete the game state to destroy all entities.
  delete (*scene)->name;
  delete (*scene)->state;
  delete *scene;
  *scene = nullptr;
  working_scene = active_ = nullptr;

  scene_activate(scene_global());
  scene_set(scene_global());

  return QB_OK;
}

qbScene PrivateUniverse::scene_global() {
  return baseline_;
}

qbResult PrivateUniverse::scene_set(qbScene scene) {
  DEBUG_OP(runner_.assert_in_state({ RunState::RUNNING, RunState::STARTED }));
  working_scene = scene;
  return QB_OK;
}

qbResult PrivateUniverse::scene_reset() {
  DEBUG_OP(runner_.assert_in_state({ RunState::RUNNING, RunState::STARTED }));
  working_scene = active_;
  return QB_OK;
}

qbResult PrivateUniverse::scene_attach(qbScene scene, const char* key, void* value) {
  scene->keys.push_back(key);
  scene->values.push_back(value);
  return QB_OK;
}

qbResult PrivateUniverse::scene_activate(qbScene scene) {
  if (active_ == scene) {
    return QB_OK;
  }

  DEBUG_OP(runner_.assert_in_state({ RunState::RUNNING, RunState::STARTED }));
  // Deactivate the currently active scene.
  if (active_) {
    for (auto& fn : active_->ondeactivate) {
      fn(scene,
         scene->keys.empty() ? 0 : scene->keys.size(),
         scene->keys.empty() ? nullptr : scene->keys.data(),
         scene->values.empty() ? nullptr : scene->values.data());
    }
  }
  
  // Activate the new scene.
  active_ = scene;
  scene_set(scene);

  for (auto& fn : scene->onactivate) {
    fn(scene,
       scene->keys.empty() ? 0 : scene->keys.size(),
       scene->keys.empty() ? nullptr : scene->keys.data(),
       scene->values.empty() ? nullptr : scene->values.data());
  }

  return QB_OK;
}

qbResult PrivateUniverse::scene_ondestroy(qbScene scene, void(*fn)(qbScene scene,
                                                                   size_t count,
                                                                   const char* keys[],
                                                                   void* values[])) {
  scene->ondestroy.push_back(fn);
  return QB_OK;
}

qbResult PrivateUniverse::scene_onactivate(qbScene scene, void(*fn)(qbScene scene,
                                                                    size_t count,
                                                                    const char* keys[],
                                                                    void* values[])) {
  scene->onactivate.push_back(fn);
  return QB_OK;
}

qbResult PrivateUniverse::scene_ondeactivate(qbScene scene, void(*fn)(qbScene scene,
                                                                      size_t count,
                                                                      const char* keys[],
                                                                      void* values[])) {
  scene->ondeactivate.push_back(fn);
  return QB_OK;
}