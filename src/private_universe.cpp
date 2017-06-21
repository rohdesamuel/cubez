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

struct qbSystem* PrivateUniverse::alloc_system(
    const char* program, const char* source, const char* sink) {
  qbProgram* p = programs_.get_program(program);
  if (!p) {
    return nullptr;
  }

  qbCollection* src = source ? collections_.get(p->id, source) : nullptr;
  qbCollection* snk = sink ? collections_.get(p->id, sink) : nullptr;

  return programs_.to_impl(p)->alloc_system(src, snk);
}

qbResult PrivateUniverse::free_system(struct qbSystem* system) {
  ASSERT_NOT_NULL(system);

  qbProgram* p = programs_.get_program(system->program);
  ASSERT_NOT_NULL(p);

  ProgramImpl* program = programs_.to_impl(p);
  return program->free_system(system);
}

struct qbSystem* PrivateUniverse::copy_system(
    struct qbSystem*, const char*) {
  return nullptr;
}

qbResult PrivateUniverse::enable_system(
    struct qbSystem* system, qbExecutionPolicy policy) {
  ASSERT_NOT_NULL(system);

  qbProgram* p = programs_.get_program(system->program);
  ASSERT_NOT_NULL(p);

  ProgramImpl* program = programs_.to_impl(p);
  return program->enable_system(system, policy);
}

qbResult PrivateUniverse::disable_system(struct qbSystem* system) {
  ASSERT_NOT_NULL(system);

  qbProgram* p = programs_.get_program(system->program);
  ASSERT_NOT_NULL(p);

  ProgramImpl* program = programs_.to_impl(p);
  return program->disable_system(system);
}

qbCollection* PrivateUniverse::alloc_collection(const char* program, const char* name) {
  qbProgram* p = programs_.get_program(program);
  ASSERT_NOT_NULL(p);

  return collections_.alloc(p->id, name);
}

qbResult PrivateUniverse::add_source(qbSystem* system, const char* source) {
  qbProgram* p = programs_.get_program(system->program);
  if (!p) {
    return QB_ERROR_NULL_POINTER;
  }

  qbCollection* src = collections_.get(p->id, source);
  return programs_.to_impl(p)->add_source(system, src);
}

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

qbId PrivateUniverse::create_event(const char* program, const char* event, qbEventPolicy policy) {
  qbProgram* p = programs_.get_program(program);
  DEBUG_ASSERT(p, QB_ERROR_NULL_POINTER);
  return programs_.to_impl(p)->create_event(event, policy);
}

qbResult PrivateUniverse::flush_events(const char* program, const char* event) {
  qbResult err = runner_.assert_in_state(RunState::RUNNING);
  if (err != QB_OK) {
    return err;
  }

  qbProgram* p = programs_.get_program(program);
  DEBUG_ASSERT(p, QB_ERROR_NULL_POINTER);
  programs_.to_impl(p)->flush_events(event);

  return QB_OK;
}

struct qbChannel* PrivateUniverse::open_channel(const char* program, const char* event) {
  qbProgram* p = programs_.get_program(program);
  DEBUG_ASSERT(p, QB_ERROR_NULL_POINTER);
  return programs_.to_impl(p)->open_channel(event);
}

void PrivateUniverse::close_channel(struct qbChannel* channel) {
  qbProgram* p = programs_.get_program(channel->program);
  DEBUG_ASSERT(p, QB_ERROR_NULL_POINTER);
  programs_.to_impl(p)->close_channel(channel);
}

struct qbSubscription* PrivateUniverse::subscribe_to(
    const char* program, const char* event, struct qbSystem* system) {
  qbProgram* p = programs_.get_program(program);
  DEBUG_ASSERT(p, QB_ERROR_NULL_POINTER);
  return programs_.to_impl(p)->subscribe_to(event, system);
}

void PrivateUniverse::unsubscribe_from(struct qbSubscription* subscription) {
  qbProgram* p = programs_.get_program(subscription->program);
  DEBUG_ASSERT(p, QB_ERROR_NULL_POINTER);
  programs_.to_impl(p)->unsubscribe_from(subscription);
}
