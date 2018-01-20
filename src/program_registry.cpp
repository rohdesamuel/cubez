#include "program_registry.h"
#include "private_universe.h"

#include <cstring>

ProgramRegistry::ProgramRegistry(ComponentRegistry* component_registry,
                                 FrameBuffer* buffer) :
    component_registry_(component_registry),
    frame_buffer_(buffer),
    program_threads_(std::thread::hardware_concurrency()) {
}

qbId ProgramRegistry::CreateProgram(const char* program) {
  std::hash<std::string> hasher;
  qbId id = hasher(program);

  DEBUG_OP(
    auto it = programs_.find(id);
    if (it != programs_.end()) {
      return -1;
    }
  );

  qbProgram* p = AllocProgram(id, program);
  programs_[id] = p;
  return id;
}

qbResult ProgramRegistry::DetatchProgram(qbId program) {
  qbProgram* to_detach = GetProgram(program);
  std::unique_ptr<ProgramThread> program_thread(
      new ProgramThread(to_detach));
  program_thread->Run();
  detached_[program] = std::move(program_thread);
  programs_.erase(programs_.find(program));
  return QB_OK;
}

qbResult ProgramRegistry::JoinProgram(qbId program) {
  programs_[program] = detached_.find(program)->second->Release();
  detached_.erase(detached_.find(program));
  return QB_OK;
}

qbProgram* ProgramRegistry::GetProgram(const char* program) {
  std::hash<std::string> hasher;
  qbId id = hasher(std::string(program));
  if (id == -1) {
    return nullptr;
  }
  return programs_[id];
}

qbProgram* ProgramRegistry::GetProgram(qbId id) {
  return programs_[id];
}

void ProgramRegistry::Run() {
  std::vector<std::future<void>> programs(programs_.size());

  frame_buffer_->ResolveDeltas();

  for (auto& program : programs_) {
    ProgramImpl::FromRaw(program.second)->Ready();
  }

  for (auto& program : programs_) {
    ProgramImpl* p = ProgramImpl::FromRaw(program.second);
    programs.push_back(program_threads_.enqueue(
      [p]() {
        PrivateUniverse::program_id = p->Id();
        p->Run();
      }));
  }

  for (auto& p : programs) {
    if (p.valid()) {
      p.wait();
    }
  }

  for (auto& program : programs_) {
    ProgramImpl::FromRaw(program.second)->Done();
  }
}

qbResult ProgramRegistry::RunProgram(qbId program) {
  ProgramImpl* p = (ProgramImpl*)programs_[program]->self;
  p->Run();
  return QB_OK;
}

qbProgram* ProgramRegistry::AllocProgram(qbId id, const char* name) {
  qbProgram* p = (qbProgram*)calloc(1, sizeof(qbProgram));
  *(qbId*)(&p->id) = id;
  *(char**)(&p->name) = STRDUP(name);
  p->self = new ProgramImpl{p, component_registry_};

  return p;
}
