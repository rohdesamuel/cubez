#include "private_universe.h"

typedef Runner::State RunState;

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
  std::lock_guard<std::mutex> lock(state_change_);
  state_ = next;
  return QB_OK;
}
qbResult Runner::transition(std::vector<State>&&, State next) {
  std::lock_guard<std::mutex> lock(state_change_);
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
  LOG(ERROR, "Runner is in bad state: " << (int)state_ << "\nAllowed to be in { ");
  DEBUG_OP(
      for (auto state : allowed) {
        LOG(ERROR, (int)state << " ");
      });
  LOG(ERROR, "}\n");
  
  return QB_ERROR_BAD_RUN_STATE;
}

qbResult Runner::assert_in_state(std::vector<State>&& allowed) {
  if (std::find(allowed.begin(), allowed.end(), state_) != allowed.end()) {
    return QB_OK;
  }
  LOG(ERROR, "Runner is in bad state: " << (int)state_ << "\nAllowed to be in { ");
  
  DEBUG_OP(
      for (auto state : allowed) {
        LOG(ERROR, (int)state << " ");
      });
  LOG(ERROR, "}\n");
  
  return QB_ERROR_BAD_RUN_STATE;
}

PrivateUniverse::PrivateUniverse() {
}

PrivateUniverse::~PrivateUniverse() {}

qbResult PrivateUniverse::init() {
  entities_.Init();
  return runner_.transition(RunState::STOPPED, RunState::INITIALIZED);
}

qbResult PrivateUniverse::start() {
  return runner_.transition(RunState::INITIALIZED, RunState::STARTED);
}

qbResult PrivateUniverse::loop() {
  runner_.transition({RunState::RUNNING, RunState::STARTED}, RunState::LOOPING);

  programs_.Run();

  return runner_.transition(RunState::LOOPING, RunState::RUNNING);
}

qbResult PrivateUniverse::stop() {
  return runner_.transition({RunState::RUNNING, RunState::UNKNOWN}, RunState::STOPPED);
}

qbId PrivateUniverse::create_program(const char* name) {
  return programs_.CreateProgram(name); 
}

qbResult PrivateUniverse::run_program(qbId program) {
  return programs_.RunProgram(program); 
}

qbResult PrivateUniverse::detach_program(qbId program) {
  return programs_.DetatchProgram(program);
}

qbResult PrivateUniverse::join_program(qbId program) {
  return programs_.JoinProgram(program);
}

qbResult PrivateUniverse::system_create(qbSystem* system, 
                                        const qbSystemAttr_& attr) {
  qbProgram* p = programs_.GetProgram(attr.program);
  if (!p) {
    return qbResult::QB_UNKNOWN;
  }

  *system = ProgramImpl::FromRaw(p)->CreateSystem(attr);

  return qbResult::QB_OK;
}

qbResult PrivateUniverse::free_system(qbSystem system) {
  ASSERT_NOT_NULL(system);

  qbProgram* p = programs_.GetProgram(system->program);
  ASSERT_NOT_NULL(p);

  return ProgramImpl::FromRaw(p)->FreeSystem(system);
}

qbSystem PrivateUniverse::copy_system(
    qbSystem, const char*) {
  return nullptr;
}

qbResult PrivateUniverse::enable_system(qbSystem system) {
  ASSERT_NOT_NULL(system);

  qbProgram* p = programs_.GetProgram(system->program);
  ASSERT_NOT_NULL(p);

  return ProgramImpl::FromRaw(p)->EnableSystem(system);
}

qbResult PrivateUniverse::disable_system(qbSystem system) {
  ASSERT_NOT_NULL(system);

  qbProgram* p = programs_.GetProgram(system->program);
  ASSERT_NOT_NULL(p);

  return ProgramImpl::FromRaw(p)->DisableSystem(system);
}


qbResult PrivateUniverse::collection_create(qbCollection* collection,
                                            qbCollectionAttr attr) {
  qbProgram* p = programs_.GetProgram(attr->program);
  ASSERT_NOT_NULL(p);
  *collection = collections_.Create(p->id, attr);
  return qbResult::QB_OK;
}

qbResult PrivateUniverse::collection_share(qbCollection, qbProgram) {
	return qbResult::QB_OK;
}

qbResult PrivateUniverse::collection_copy(qbCollection, qbProgram) {
	return qbResult::QB_OK;
}

qbResult PrivateUniverse::collection_destroy(qbCollection*) {
	return qbResult::QB_OK;
}

#if 0
qbResult PrivateUniverse::share_collection(
      const char* source_program, const char* source_collection,
      const char* dest_program, const char* dest_collection) {
  qbProgram* src = programs_.get_program(source_program);
  if (!src) {
    return QB_ERROR_NULL_POINTER;
  }
  qbProgram* dst = programs_.get_program(dest_program);
  if (!dst) {
    return QB_ERROR_NULL_POINTER;
  }

  return collections_.share(src->id, source_collection,
                            dst->id, dest_collection);
}

qbResult PrivateUniverse::copy_collection(const char*, const char*,
                                          const char*, const char*) {
  return QB_OK;
}
#endif

qbResult PrivateUniverse::event_create(qbEvent* event, qbEventAttr attr) {
  qbProgram* p = programs_.GetProgram(attr->program);
  DEBUG_ASSERT(p, QB_ERROR_NULL_POINTER);
  return ProgramImpl::FromRaw(p)->CreateEvent(event, attr);
}

qbResult PrivateUniverse::event_destroy(qbEvent*) {
	return qbResult::QB_OK;
}

qbResult PrivateUniverse::event_flush(qbEvent event) {
  qbResult err = runner_.assert_in_state(RunState::RUNNING);
  if (err != QB_OK) {
    return err;
  }

  qbProgram* p = programs_.GetProgram(event->program);
  DEBUG_ASSERT(p, QB_ERROR_NULL_POINTER);
  ProgramImpl::FromRaw(p)->FlushEvent(event);
	return qbResult::QB_OK;
}

qbResult PrivateUniverse::event_flushall(qbProgram program) {
  qbResult err = runner_.assert_in_state(RunState::RUNNING);
  if (err != QB_OK) {
    return err;
  }

  qbProgram* p = programs_.GetProgram(program.id);
  DEBUG_ASSERT(p, QB_ERROR_NULL_POINTER);
  ProgramImpl::FromRaw(p)->FlushAllEvents();
	return qbResult::QB_OK;
}

qbResult PrivateUniverse::event_subscribe(qbEvent event, qbSystem system) {
  qbProgram* p = programs_.GetProgram(event->program);
  ProgramImpl::FromRaw(p)->SubscribeTo(event, system);
	return qbResult::QB_OK;
}

qbResult PrivateUniverse::event_unsubscribe(qbEvent event, qbSystem system) {
  qbProgram* p = programs_.GetProgram(event->program);
  ProgramImpl::FromRaw(p)->UnsubscribeFrom(event, system);
	return qbResult::QB_OK;
}

qbResult PrivateUniverse::event_send(qbEvent event, void* message) {
  return ((Channel*)event->channel)->SendMessage(message);
}

qbResult PrivateUniverse::event_sendsync(qbEvent event, void* message) {
  return ((Channel*)event->channel)->SendMessage(message);
}

qbResult PrivateUniverse::entity_create(qbEntity* entity, const qbEntityAttr_& attr) {
  return entities_.CreateEntity(entity, attr);
}

qbResult PrivateUniverse::entity_destroy(qbEntity* entity) {
  return entities_.DestroyEntity(entity);
}

qbResult PrivateUniverse::entity_find(qbEntity* entity, qbId entity_id) {
  return entities_.Find(entity, entity_id);
}

qbResult PrivateUniverse::entity_addcomponent(qbEntity entity,
                                             qbComponent component,
                                             void* instance_data) {
  return entities_.AddComponent(entity, component, instance_data);
}

qbResult PrivateUniverse::entity_removecomponent(qbEntity entity,
                                                qbComponent component) {
  return entities_.RemoveComponent(entity, component);
}

qbResult PrivateUniverse::component_create(qbComponent* component, qbComponentAttr attr) {
  return components_.CreateComponent(component, attr);
}
