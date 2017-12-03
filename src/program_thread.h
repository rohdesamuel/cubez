#ifndef PROGRAM_THREAD__H
#define PROGRAM_THREAD__H

#include "defs.h"

#include <atomic>
#include <memory>
#include <thread>


class ProgramThread {
 public:
  ProgramThread(qbProgram* program);

  ~ProgramThread();

  void Run();
  
  qbProgram* Release();

 private:
  qbProgram* program_;
  std::unique_ptr<std::thread> thread_;
  std::atomic_bool is_running_;
};

#endif  // PROGRAM_THREAD__H

