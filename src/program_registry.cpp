#include "program_registry.h"

#include <cstring>

ProgramRegistry::ProgramRegistry() :
    program_threads_(std::thread::hardware_concurrency()) {
  // Create the default program.
  CreateProgram("");
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
  std::vector<std::future<void>> programs;

  for (auto& program : programs_) {
    ProgramImpl* p = (ProgramImpl*)program.second->self;
    programs.push_back(program_threads_.enqueue(
          [p]() { p->Run(); }));
  }

  for (auto& p : programs) {
    p.wait();
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
  *(char**)(&p->name) = strdup(name);
  p->self = new ProgramImpl{p};

  return p;
}
