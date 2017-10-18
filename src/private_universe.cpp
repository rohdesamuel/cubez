#include "private_universe.h"

#include <algorithm>

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
  //init_rendering(&rendering_context_,
      //{ GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None }, 600, 600);
}

PrivateUniverse::~PrivateUniverse() {}

qbResult PrivateUniverse::init() {
  // Create the default program.
  create_program("");
  return runner_.transition(RunState::STOPPED, RunState::INITIALIZED);
}

qbResult PrivateUniverse::start() {
  return runner_.transition(RunState::INITIALIZED, RunState::STARTED);
}

qbResult PrivateUniverse::loop() {
  runner_.transition({RunState::RUNNING, RunState::STARTED}, RunState::LOOPING);

  programs_.run();

  return runner_.transition(RunState::LOOPING, RunState::RUNNING);
}

qbResult PrivateUniverse::stop() {
  return runner_.transition({RunState::RUNNING, RunState::UNKNOWN}, RunState::STOPPED);
}

qbId PrivateUniverse::create_program(const char* name) {
  return programs_.create_program(name); 
}

qbResult PrivateUniverse::run_program(qbId program) {
  return programs_.run_program(program); 
}

qbResult PrivateUniverse::detach_program(qbId program) {
  return programs_.detach_program(program);
}

qbResult PrivateUniverse::join_program(qbId program) {
  return programs_.join_program(program);
}

qbResult PrivateUniverse::system_create(qbSystem* system, 
                                        const qbSystemAttr_& attr) {
  qbProgram* p = programs_.get_program(attr.program);
  if (!p) {
    return qbResult::QB_UNKNOWN;
  }

  *system = programs_.to_impl(p)->create_system(attr);

  return qbResult::QB_OK;
}

qbResult PrivateUniverse::free_system(qbSystem system) {
  ASSERT_NOT_NULL(system);

  qbProgram* p = programs_.get_program(system->program);
  ASSERT_NOT_NULL(p);

  ProgramImpl* program = programs_.to_impl(p);
  return program->free_system(system);
}

qbSystem PrivateUniverse::copy_system(
    qbSystem, const char*) {
  return nullptr;
}

qbResult PrivateUniverse::enable_system(qbSystem system) {
  ASSERT_NOT_NULL(system);

  qbProgram* p = programs_.get_program(system->program);
  ASSERT_NOT_NULL(p);

  ProgramImpl* program = programs_.to_impl(p);
  return program->enable_system(system);
}

qbResult PrivateUniverse::disable_system(qbSystem system) {
  ASSERT_NOT_NULL(system);

  qbProgram* p = programs_.get_program(system->program);
  ASSERT_NOT_NULL(p);

  ProgramImpl* program = programs_.to_impl(p);
  return program->disable_system(system);
}


qbResult PrivateUniverse::collection_create(qbCollection* collection,
                                            qbCollectionAttr attr) {
  qbProgram* p = programs_.get_program(attr->program);
  ASSERT_NOT_NULL(p);
  *collection = collections_.alloc(p->id, attr);
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
  qbProgram* p = programs_.get_program(attr->program);
  DEBUG_ASSERT(p, QB_ERROR_NULL_POINTER);
  return programs_.to_impl(p)->create_event(event, attr);
}

qbResult PrivateUniverse::event_destroy(qbEvent*) {
	return qbResult::QB_OK;
}

qbResult PrivateUniverse::event_flush(qbEvent event) {
  qbResult err = runner_.assert_in_state(RunState::RUNNING);
  if (err != QB_OK) {
    return err;
  }

  qbProgram* p = programs_.get_program(event->program);
  DEBUG_ASSERT(p, QB_ERROR_NULL_POINTER);
  programs_.to_impl(p)->flush_events(event);
	return qbResult::QB_OK;
}

qbResult PrivateUniverse::event_flushall(qbProgram program) {
  qbResult err = runner_.assert_in_state(RunState::RUNNING);
  if (err != QB_OK) {
    return err;
  }

  qbProgram* p = programs_.get_program(program.id);
  DEBUG_ASSERT(p, QB_ERROR_NULL_POINTER);
  programs_.to_impl(p)->flush_all_events();
	return qbResult::QB_OK;
}

qbResult PrivateUniverse::event_subscribe(qbEvent event, qbSystem system) {
  qbProgram* p = programs_.get_program(event->program);
  programs_.to_impl(p)->subscribe_to(event, system);
	return qbResult::QB_OK;
}

qbResult PrivateUniverse::event_unsubscribe(qbEvent event, qbSystem system) {
  qbProgram* p = programs_.get_program(event->program);
  programs_.to_impl(p)->unsubscribe_from(event, system);
	return qbResult::QB_OK;
}

qbResult PrivateUniverse::event_send(qbEvent event, void* message) {
  return ((Channel*)event->channel)->send_message(message);
}

qbResult PrivateUniverse::event_sendsync(qbEvent event, void* message) {
  return ((Channel*)event->channel)->send_message(message);
}

qbResult PrivateUniverse::entity_create(qbEntity* entity, const qbEntityAttr_& attr) {
  if (runner_.state() == Runner::State::LOOPING) {
    return entities_.create_entityasync(entity, attr);
  }
  return entities_.create_entity(entity, attr);
}

qbResult PrivateUniverse::component_create(qbComponent* component, qbComponentAttr attr) {
  return components_.create_component(component, attr);
}
