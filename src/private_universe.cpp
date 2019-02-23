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

PrivateUniverse::PrivateUniverse()
  : baseline_(std::make_unique<EntityRegistry>(),
              std::make_unique<ComponentRegistry>()) {
  programs_ = std::make_unique<ProgramRegistry>();

  // Create the default program.
  create_program("");
}

PrivateUniverse::~PrivateUniverse() {}

qbResult PrivateUniverse::init() {
  return runner_.transition(RunState::STOPPED, RunState::INITIALIZED);
}

qbResult PrivateUniverse::start() {
  return runner_.transition(RunState::INITIALIZED, RunState::STARTED);
}

qbResult PrivateUniverse::loop() {
  runner_.transition({RunState::RUNNING, RunState::STARTED}, RunState::LOOPING);

  Baseline()->Flush();
  programs_->Run(Baseline());

  return runner_.transition(RunState::LOOPING, RunState::RUNNING);
}

qbResult PrivateUniverse::stop() {
  return runner_.transition({RunState::RUNNING, RunState::UNKNOWN}, RunState::STOPPED);
}

qbId PrivateUniverse::create_program(const char* name) {
  return programs_->CreateProgram(name);
}

qbResult PrivateUniverse::run_program(qbId program) {
  return programs_->RunProgram(program, Baseline());
}

qbResult PrivateUniverse::detach_program(qbId program) {
  return programs_->DetatchProgram(program, [this]() { return Baseline(); });
}

qbResult PrivateUniverse::join_program(qbId program) {
  return programs_->JoinProgram(program);
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

qbResult PrivateUniverse::event_create(qbEvent* event, qbEventAttr attr) {
  qbProgram* p = programs_->GetProgram(attr->program);
  DEBUG_ASSERT(p, QB_ERROR_NULL_POINTER);
  return ProgramImpl::FromRaw(p)->CreateEvent(event, attr);
}

qbResult PrivateUniverse::event_destroy(qbEvent*) {
	return qbResult::QB_OK;
}

qbResult PrivateUniverse::event_flushall(qbProgram program) {
  qbResult err = runner_.assert_in_state(RunState::RUNNING);
  if (err != QB_OK) {
    return err;
  }

  qbProgram* p = programs_->GetProgram(program.id);
  DEBUG_ASSERT(p, QB_ERROR_NULL_POINTER);
  ProgramImpl::FromRaw(p)->FlushAllEvents(Baseline());
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
  return ((Event*)event->event)->SendMessageSync(message, Baseline());
}

qbResult PrivateUniverse::entity_create(qbEntity* entity, const qbEntityAttr_& attr) {
  return Baseline()->EntityCreate(entity, attr);
}

qbResult PrivateUniverse::entity_destroy(qbEntity entity) {
  return Baseline()->EntityDestroy(entity);
}

qbResult PrivateUniverse::entity_find(qbEntity* entity, qbId entity_id) {
  return Baseline()->EntityFind(entity, entity_id);
}

bool PrivateUniverse::entity_hascomponent(qbEntity entity,
                                          qbComponent component) {
  return Baseline()->EntityHasComponent(entity, component);
}

qbResult PrivateUniverse::entity_addcomponent(qbEntity entity,
                                              qbComponent component,
                                              void* instance_data) {
  return Baseline()->EntityAddComponent(entity, component,
                                                       instance_data);
}

qbResult PrivateUniverse::entity_removecomponent(qbEntity entity,
                                                qbComponent component) {
  return Baseline()->EntityRemoveComponent(entity, component);
}

qbResult PrivateUniverse::component_create(qbComponent* component, qbComponentAttr attr) {
  return Baseline()->ComponentCreate(component, attr);
}

size_t PrivateUniverse::component_getcount(qbComponent component) {
  return Baseline()->ComponentGetCount(component);
}

qbResult PrivateUniverse::instance_oncreate(qbComponent component,
                                            qbInstanceOnCreate on_create) {
  qbSystemAttr attr;
  qb_systemattr_create(&attr);
  qb_systemattr_settrigger(attr, qbTrigger::QB_TRIGGER_EVENT);
  qb_systemattr_setuserstate(attr, (void*)on_create);
  qb_systemattr_setcallback(attr, [](qbFrame* frame) {
    qbInstanceOnCreateEvent_* event =
      (qbInstanceOnCreateEvent_*)frame->event;
    qbInstanceOnCreate on_create = (qbInstanceOnCreate)frame->state;
    qbInstance_ instance = SystemImpl::FromRaw(frame->system)->FindInstance(
      event->entity, event->component, event->state);
    on_create(&instance);
  });
  qbSystem system;
  qb_system_create(&system, attr);

  Baseline()->ComponentSubscribeToOnCreate(system, component);
  qb_systemattr_destroy(&attr);
  return QB_OK;
}

qbResult PrivateUniverse::instance_ondestroy(qbComponent component,
                                             qbInstanceOnDestroy on_destroy) {
  qbSystemAttr attr;
  qb_systemattr_create(&attr);
  qb_systemattr_settrigger(attr, qbTrigger::QB_TRIGGER_EVENT);
  qb_systemattr_setuserstate(attr, (void*)on_destroy);
  qb_systemattr_setcallback(attr, [](qbFrame* frame) {
    qbInstanceOnDestroyEvent_* event =
      (qbInstanceOnDestroyEvent_*)frame->event;
    qbInstanceOnDestroy on_destroy = (qbInstanceOnDestroy)frame->state;
    qbInstance_ instance = SystemImpl::FromRaw(frame->system)->FindInstance(
      event->entity, event->component, event->state);
    on_destroy(&instance);
  });
  qbSystem system;
  qb_system_create(&system, attr);

  Baseline()->ComponentSubscribeToOnDestroy(system, component);
  qb_systemattr_destroy(&attr);

  return QB_OK;
}

qbResult PrivateUniverse::instance_getconst(qbInstance instance, void* pbuffer) {
  if (instance->is_mutable) {
    *(void**)pbuffer = nullptr;
  } else {
    *(void**)pbuffer = instance->data;
  }
  return QB_OK;
}

qbResult PrivateUniverse::instance_getmutable(qbInstance instance, void* pbuffer) {
  if (instance->is_mutable) {
    *(void**)pbuffer = instance->data;
  } else {
    *(void**)pbuffer = nullptr;
  }
  return QB_OK;
}

qbResult PrivateUniverse::instance_getcomponent(qbInstance instance,
                                                qbComponent component,
                                                void* pbuffer) {
  *(void**)pbuffer = Baseline()->ComponentGetEntityData(component, instance->entity);
  return QB_OK;
}

bool PrivateUniverse::instance_hascomponent(qbInstance instance, qbComponent component) {
  return Baseline()->EntityHasComponent(instance->entity, component);
}

qbResult PrivateUniverse::instance_find(qbComponent component, qbEntity entity, void* pbuffer) {
  *(void**)pbuffer = Baseline()->ComponentGetEntityData(component, entity);
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