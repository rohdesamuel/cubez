#include "program_registry.h"
#include "private_universe.h"

#include <cstring>

ProgramRegistry::ProgramRegistry() :
    program_threads_(std::thread::hardware_concurrency()) {
  id_ = 0;
}

qbId ProgramRegistry::CreateProgram(const char* program) {
  qbId id = id_++;

  qbProgram* p = AllocProgram(id, program);
  programs_[id] = p;
  return id;
}

qbResult ProgramRegistry::DetatchProgram(qbId program, const std::function<GameState*()>& game_state_fn) {
  if (programs_.has(program)) {
    qbProgram* to_detach = GetProgram(program);
    std::unique_ptr<ProgramThread> program_thread(
      new ProgramThread(to_detach));
    program_thread->Run(game_state_fn);
    detached_[program] = std::move(program_thread);
    programs_.erase(program);
  }
  return QB_OK;
}

qbResult ProgramRegistry::JoinProgram(qbId program) {
  programs_[program] = detached_.find(program)->second->Release();
  detached_.erase(detached_.find(program));
  return QB_OK;
}

qbProgram* ProgramRegistry::GetProgram(qbId id) {
  if (!programs_.has(id)) {
    return nullptr;
  }
  return programs_[id];
}

void ProgramRegistry::Run(GameState* state) {
  std::vector<std::future<void>> programs(programs_.size());

  for (auto program : programs_) {
    ProgramImpl::FromRaw(program.second)->Ready();
  }

  for (auto program : programs_) {
    ProgramImpl* p = ProgramImpl::FromRaw(program.second);
    programs.push_back(program_threads_.enqueue(
      [p, state]() {
        PrivateUniverse::program_id = p->Id();
        p->Run(state);
      }));
  }

  for (auto& p : programs) {
    if (p.valid()) {
      p.wait();
    }
  }

  for (auto program : programs_) {
    ProgramImpl::FromRaw(program.second)->Done();
  }
}

qbResult ProgramRegistry::RunProgram(qbId program, GameState* state) {
  ProgramImpl* p = (ProgramImpl*)programs_[program]->self;
  p->Run(state);
  return QB_OK;
}

qbProgram* ProgramRegistry::AllocProgram(qbId id, const char* name) {
  qbProgram* p = (qbProgram*)calloc(1, sizeof(qbProgram));
  *(qbId*)(&p->id) = id;
  *(char**)(&p->name) = STRDUP(name);
  p->self = new ProgramImpl(p);

  return p;
}
