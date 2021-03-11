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

#ifdef __COMPILE_AS_WINDOWS__
#undef CreateEvent
#undef SendMessage
#endif

typedef Runner::State RunState;

thread_local qbId PrivateUniverse::program_id;

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
      std::string allowed_states = ""
      for (auto state : allowed) {
        allowed_states += state + "\n";
      }
      INFO("Runner is in bad state: " << (int)state_ << "\nAllowed to be in { " << allowed_states << "\n");
  );
  
  return QB_ERROR_BAD_RUN_STATE;
}

qbResult Runner::assert_in_state(std::vector<State>&& allowed) {
  if (std::find(allowed.begin(), allowed.end(), state_) != allowed.end()) {
    return QB_OK;
  }
  DEBUG_OP(
    std::string allowed_states = ""
    for (auto state : allowed) {
      allowed_states += state + "\n";
    }
  INFO("Runner is in bad state: " << (int)state_ << "\nAllowed to be in { " << allowed_states << "\n");
  );
  
  return QB_ERROR_BAD_RUN_STATE;
}

PrivateUniverse::PrivateUniverse() {
  programs_ = std::make_unique<ProgramRegistry>();
  components_ = std::make_unique<ComponentRegistry>();

  scene_create(&baseline_, "");
  working_ = active_ = baseline_;

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

  WorkingScene()->Flush();
  programs_->Run(WorkingScene());

  return runner_.transition(RunState::LOOPING, RunState::RUNNING);
}

qbResult PrivateUniverse::stop() {
  return runner_.transition({RunState::RUNNING, RunState::UNKNOWN}, RunState::STOPPED);
}

qbId PrivateUniverse::create_program(const char* name) {
  return programs_->CreateProgram(name);
}

qbResult PrivateUniverse::run_program(qbId program) {
  return programs_->RunProgram(program, WorkingScene());
}

qbResult PrivateUniverse::detach_program(qbId program) {
  return programs_->DetatchProgram(program, [this]() { return WorkingScene(); });
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

qbResult PrivateUniverse::foreach_system(qbComponent* components, size_t component_count,
                                         qbVar var, void(*fn)(qbInstance*, qbVar)) {
  qbSystemAttr_ attr = {};

  qb_systemattr_setjoin(&attr, QB_JOIN_LEFT);

  std::vector<qbComponent> components_v;
  components_v.reserve(component_count);

  for (size_t i = 0; i < component_count; ++i) {
    qb_systemattr_addmutable(&attr, components[i]);
    components_v.push_back(components[i]);
  }

  struct ForEachState {
    void(*fn)(qbInstance*, qbVar);
    qbVar var;
  };

  ForEachState state{ fn, var };

  qb_systemattr_setuserstate(&attr, &state);
  qb_systemattr_setfunction(&attr, [](qbInstance* insts, qbFrame* frame) {
    ForEachState* state = (ForEachState*)frame->state;
    state->fn(insts, state->var);
  });

  SystemImpl s(attr, nullptr, components_v);
  s.Run(active_->state);

  return QB_OK;
}

qbResult PrivateUniverse::event_create(qbEvent* event, qbEventAttr attr) {
  qbProgram* p = programs_->GetProgram(attr->program);
  DEBUG_ASSERT(p, QB_ERROR_NULL_POINTER);
  return ProgramImpl::FromRaw(p)->CreateEvent(event, attr);
}

qbResult PrivateUniverse::event_destroy(qbEvent*) {
	return qbResult::QB_OK;
}

qbResult PrivateUniverse::event_flushall(qbProgram program) {
  DEBUG_OP(
    qbResult err = runner_.assert_in_state(RunState::RUNNING);
    if (err != QB_OK) {
      return err;
    }
  );

  qbProgram* p = programs_->GetProgram(program.id);
  DEBUG_ASSERT(p, QB_ERROR_NULL_POINTER);
  ProgramImpl::FromRaw(p)->FlushAllEvents(WorkingScene());
	return qbResult::QB_OK;
}

qbResult PrivateUniverse::event_subscribe(qbEvent event, qbSystem system) {
  qbProgram* p = programs_->GetProgram(event->program);
  ProgramImpl::FromRaw(p)->SubscribeTo(event, system);
	return qbResult::QB_OK;
}

qbResult PrivateUniverse::event_unsubscribe(qbEvent event, qbSystem system) {
  qbProgram* p = programs_->GetProgram(event->program);
  ProgramImpl::FromRaw(p)->UnsubscribeFrom(event, system);
	return qbResult::QB_OK;
}

qbResult PrivateUniverse::event_send(qbEvent event, void* message) {
  return ((Event*)event->event)->SendMessage(message);
}

qbResult PrivateUniverse::event_sendsync(qbEvent event, void* message) {
  return ((Event*)event->event)->SendMessageSync(message, WorkingScene());
}

qbResult PrivateUniverse::entity_create(qbEntity* entity, const qbEntityAttr_& attr) {
  return WorkingScene()->EntityCreate(entity, attr);
}

qbResult PrivateUniverse::entity_destroy(qbEntity entity) {
  return WorkingScene()->EntityDestroy(entity);
}

qbResult PrivateUniverse::entity_find(qbEntity* entity, qbId entity_id) {
  return WorkingScene()->EntityFind(entity, entity_id);
}

bool PrivateUniverse::entity_hascomponent(qbEntity entity,
                                          qbComponent component) {
  return WorkingScene()->EntityHasComponent(entity, component);
}

qbResult PrivateUniverse::entity_addcomponent(qbEntity entity,
                                              qbComponent component,
                                              void* instance_data) {
  return WorkingScene()->EntityAddComponent(entity, component,
                                                       instance_data);
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

qbResult PrivateUniverse::component_oncreate(qbComponent component, qbSystem system) {
  WorkingScene()->ComponentSubscribeToOnCreate(system, component);
  return QB_OK;
}

qbResult PrivateUniverse::component_ondestroy(qbComponent component, qbSystem system) {
  WorkingScene()->ComponentSubscribeToOnDestroy(system, component);
  return QB_OK;
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

  qbSystemAttr attr;
  qb_systemattr_create(&attr);
  qb_systemattr_settrigger(attr, qbTrigger::QB_TRIGGER_EVENT);
  qb_systemattr_setuserstate(attr, fn_state);
  qb_systemattr_setcallback(attr, [](qbFrame* frame) {
    qbInstanceOnCreateEvent_* event =
      (qbInstanceOnCreateEvent_*)frame->event;
    qbInstanceOnCreateState* fn_state = (qbInstanceOnCreateState*)frame->state;
    qbInstance_ instance = SystemImpl::FromRaw(frame->system)->FindInstance(
      event->entity, event->component, event->state);
    fn_state->on_create(&instance, fn_state->state);
  });
  qbSystem system;
  qb_system_create(&system, attr);

  WorkingScene()->ComponentSubscribeToOnCreate(system, component);
  qb_systemattr_destroy(&attr);
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

  qbSystemAttr attr;
  qb_systemattr_create(&attr);
  qb_systemattr_settrigger(attr, qbTrigger::QB_TRIGGER_EVENT);
  qb_systemattr_setuserstate(attr, fn_state);
  qb_systemattr_setcallback(attr, [](qbFrame* frame) {
    qbInstanceOnDestroyEvent_* event =
      (qbInstanceOnDestroyEvent_*)frame->event;
    qbInstanceOnDestroyState* fn_state = (qbInstanceOnDestroyState*)frame->state;
    qbInstance_ instance = SystemImpl::FromRaw(frame->system)->FindInstance(
      event->entity, event->component, event->state);
    fn_state->on_destroy(&instance, fn_state->state);
  });
  qbSystem system;
  qb_system_create(&system, attr);

  WorkingScene()->ComponentSubscribeToOnDestroy(system, component);
  qb_systemattr_destroy(&attr);

  return QB_OK;
}

qbResult PrivateUniverse::instance_getconst(qbInstance instance, void* pbuffer) {
  if (instance->is_mutable) {
    *(void**)pbuffer = nullptr;
  } else {
    if (instance->has_schema) {
      *(void**)pbuffer = &((qbStruct_*)instance->data)->data;
    } else {
      *(void**)pbuffer = instance->data;
    }
  }
  return QB_OK;
}

qbResult PrivateUniverse::instance_getmutable(qbInstance instance, void* pbuffer) {
  if (instance->is_mutable) {
    if (instance->has_schema) {
      *(void**)pbuffer = &((qbStruct_*)instance->data)->data;
    } else {
      *(void**)pbuffer = instance->data;
    }
  } else {
    *(void**)pbuffer = nullptr;
  }
  return QB_OK;
}

qbResult PrivateUniverse::instance_getcomponent(qbInstance instance,
                                                qbComponent component,
                                                void* pbuffer) {
  *(void**)pbuffer = WorkingScene()->ComponentGetEntityData(component, instance->entity);
  return QB_OK;
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
  barriers_.erase(std::find(barriers_.begin(), barriers_.end(), barrier));
  delete (Barrier*)(barrier)->impl;
  delete barrier;
}

qbResult PrivateUniverse::scene_create(qbScene* scene, const char* name) {
  if (scene_global() && *scene == scene_global()) {
    return QB_OK;
  }

  qbScene ret = new qbScene_();
  ret->state = new GameState(std::make_unique<EntityRegistry>(),
                             std::make_unique<InstanceRegistry>(*components_),
                             components_.get());
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
  working_ = active_ = nullptr;

  scene_activate(scene_global());
  scene_set(scene_global());

  return QB_OK;
}

qbScene PrivateUniverse::scene_global() {
  return baseline_;
}

qbResult PrivateUniverse::scene_set(qbScene scene) {
  DEBUG_OP(runner_.assert_in_state({ RunState::RUNNING, RunState::STARTED }));
  working_ = scene;
  return QB_OK;
}

qbResult PrivateUniverse::scene_reset() {
  DEBUG_OP(runner_.assert_in_state({ RunState::RUNNING, RunState::STARTED }));
  working_ = active_;
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