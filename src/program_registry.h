#ifndef PROGRAM_REGISTRY__H
#define PROGRAM_REGISTRY__H

#include "defs.h"
#include "program_impl.h"
#include "program_thread.h"
#include "thread_pool.h"

#include <algorithm>
#include <future>
#include <map>

class ProgramRegistry {
 public:
  ProgramRegistry();

  qbId CreateProgram(const char* program);

  qbResult DetatchProgram(qbId program, const std::function<GameState*()>& game_state_fn);

  qbResult JoinProgram(qbId program);

  qbProgram* GetProgram(qbId id);

  void Run(GameState* state);

  qbResult RunProgram(qbId program, GameState* state);

 private:
  qbProgram* AllocProgram(qbId id, const char* name);

  std::atomic_long id_;
  SparseMap<qbProgram*, std::vector<qbProgram*>> programs_;
  std::unordered_map<size_t, std::unique_ptr<ProgramThread>> detached_;
  ThreadPool program_threads_;
};

#endif  // PROGRAM_REGISTRY__H

