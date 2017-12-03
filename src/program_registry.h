#ifndef PROGRAM_REGISTRY__H
#define PROGRAM_REGISTRY__H

#include "defs.h"
#include "program_impl.h"
#include "program_thread.h"
#include "thread_pool.h"

#include <algorithm>
#include <future>

class ProgramRegistry {
 public:
  ProgramRegistry();

  qbId CreateProgram(const char* program);

  qbResult DetatchProgram(qbId program);

  qbResult JoinProgram(qbId program);

  qbProgram* GetProgram(const char* program);

  qbProgram* GetProgram(qbId id);

  void Run();

  qbResult RunProgram(qbId program);

 private:
  qbProgram* AllocProgram(qbId id, const char* name);

  std::unordered_map<size_t, qbProgram*> programs_;
  std::unordered_map<size_t, std::unique_ptr<ProgramThread>> detached_;
  ThreadPool program_threads_;
};

#endif  // PROGRAM_REGISTRY__H

