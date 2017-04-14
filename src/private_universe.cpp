#include "private_universe.h"

#include <algorithm>

namespace {
  std::string form_path(const char* program, const char* resource) {
    return std::string{program} + cubez::NAMESPACE_DELIMETER + std::string{resource};
  }
}  // namespace

namespace cubez {

typedef Runner::State RunState;

void Runner::wait_until(const std::vector<State>& allowed) {
  while (std::find(allowed.begin(), allowed.end(), state_) == allowed.end());
}

void Runner::wait_until(std::vector<State>&& allowed) {
  while (std::find(allowed.begin(), allowed.end(), state_) == allowed.end());
}

Status::Code Runner::transition(
    State allowed, State next) {
  return transition(std::vector<State>{allowed}, next);
}


#ifdef __ENGINE_DEBUG__
Status::Code Runner::transition(const std::vector<State>& allowed, State next) {
  std::lock_guard<std::mutex> lock(state_change_);
  if (assert_in_state(allowed) == Status::OK) {
    state_ = next;
    return Status::OK;
  }
  state_ = State::ERROR;
  return Status::BAD_RUN_STATE;
}

Status::Code Runner::transition(
    std::vector<State>&& allowed, State next) {
  std::lock_guard<std::mutex> lock(state_change_);
  if (assert_in_state(std::move(allowed)) == Status::OK) {
    state_ = next;
    return Status::OK;
  }
  state_ = State::ERROR;
  return Status::BAD_RUN_STATE;
}
#else
Status::Code Runner::transition(const std::vector<State>&, State next) {
  std::lock_guard<std::mutex> lock(state_change_);
  state_ = next;
  return Status::OK;
}
Status::Code Runner::transition(std::vector<State>&&, State next) {
  std::lock_guard<std::mutex> lock(state_change_);
  state_ = next;
  return Status::OK;
}
#endif


Status::Code Runner::assert_in_state(State allowed) {
  return assert_in_state(std::vector<State>{allowed});
}

Status::Code Runner::assert_in_state(const std::vector<State>& allowed) {
  if (std::find(allowed.begin(), allowed.end(), state_) != allowed.end()) {
    return Status::OK;
  }
  std::cerr << "Runner is in bad state: " << (int)state_
            << "\nAllowed to be in { ";
  for (auto state : allowed) {
    std::cerr << (int)state << " ";
  }
  std::cerr << "}\n";
  
  return Status::BAD_RUN_STATE;
}

Status::Code Runner::assert_in_state(std::vector<State>&& allowed) {
  if (std::find(allowed.begin(), allowed.end(), state_) != allowed.end()) {
    return Status::OK;
  }
  std::cerr << "Runner is in bad state: " << (int)state_
            << "\nAllowed to be in { ";
  for (auto state : allowed) {
    std::cerr << (int)state << " ";
  }
  std::cerr << "}\n";
  
  return Status::BAD_RUN_STATE;
}

PrivateUniverse::PrivateUniverse() {
  //init_rendering(&rendering_context_,
      //{ GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None }, 600, 600);
}

PrivateUniverse::~PrivateUniverse() {}

Status::Code PrivateUniverse::init() {
  return runner_.transition(RunState::STOPPED, RunState::INITIALIZED);
}

Status::Code PrivateUniverse::start() {
  return runner_.transition(RunState::INITIALIZED, RunState::STARTED);
}

Status::Code PrivateUniverse::loop() {
  runner_.transition({RunState::RUNNING, RunState::STARTED}, RunState::LOOPING);

  ProgramImpl* p = (ProgramImpl*)programs_.get_program("main")->self;
  p->run();

  return runner_.transition(RunState::LOOPING, RunState::RUNNING);
}

Status::Code PrivateUniverse::stop() {
  return runner_.transition({RunState::RUNNING, RunState::UNKNOWN}, RunState::STOPPED);
}

Id PrivateUniverse::create_program(const char* name) {
  return programs_.create_program(name); 
}

Id PrivateUniverse::detach_program(const char* name) {
  Program* to_detach = programs_.get_program(name);
  if (to_detach) {
    programs_.to_impl(to_detach)->detach();
    return to_detach->id;
  }
  return -1;
}

Id PrivateUniverse::join_program(const char*){ return -1; }

struct Pipeline* PrivateUniverse::add_pipeline(
    const char* program, const char* source, const char* sink) {
  Program* p = programs_.get_program(program);
  if (!p) {
    return nullptr;
  }

  Collection* src = source ? collections_.get(form_path(program, source).data()) : nullptr;
  Collection* snk = sink ? collections_.get(form_path(program, sink).data()) : nullptr;

  return programs_.to_impl(p)->add_pipeline(src, snk);
}

Status::Code PrivateUniverse::remove_pipeline(struct Pipeline* pipeline) {
  ASSERT_NOT_NULL(pipeline);

  Program* p = programs_.get_program(pipeline->program);
  ASSERT_NOT_NULL(p);

  ProgramImpl* program = programs_.to_impl(p);
  return program->remove_pipeline(pipeline);
}

struct Pipeline* PrivateUniverse::copy_pipeline(
    struct Pipeline*, const char*) {
  return nullptr;
}

Status::Code PrivateUniverse::enable_pipeline(
    struct Pipeline* pipeline, ExecutionPolicy policy) {
  ASSERT_NOT_NULL(pipeline);

  Program* p = programs_.get_program(pipeline->program);
  ASSERT_NOT_NULL(p);

  ProgramImpl* program = programs_.to_impl(p);
  return program->enable_pipeline(pipeline, policy);
}

Status::Code PrivateUniverse::disable_pipeline(struct Pipeline* pipeline) {
  ASSERT_NOT_NULL(pipeline);

  Program* p = programs_.get_program(pipeline->program);
  ASSERT_NOT_NULL(p);

  ProgramImpl* program = programs_.to_impl(p);
  return program->disable_pipeline(pipeline);
}

Collection* PrivateUniverse::add_collection(const char* program, const char* name) {
  return collections_.add(program, name);
}

Status::Code PrivateUniverse::add_source(Pipeline* pipeline, const char* source) {
  Program* p = programs_.get_program(pipeline->program);
  if (!p) {
    return Status::NULL_POINTER;
  }

  Collection* src = collections_.get(source);
  return programs_.to_impl(p)->add_source(pipeline, src);
}

Status::Code PrivateUniverse::add_sink(Pipeline* pipeline, const char* sink) {
  Program* p = programs_.get_program(pipeline->program);
  if (!p) {
    return Status::NULL_POINTER;
  }

  Collection* snk = collections_.get(sink);
  return programs_.to_impl(p)->add_source(pipeline, snk);
}

Status::Code PrivateUniverse::share_collection(const char* source, const char* dest) {
  return collections_.share(source, dest);
}

Status::Code PrivateUniverse::copy_collection(const char*, const char*) {
  return Status::OK;
}

}  // namespace cubez
