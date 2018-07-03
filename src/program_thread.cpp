#include "program_thread.h"

#include "program_impl.h"

ProgramThread::ProgramThread(qbProgram* program) :
    program_(program), is_running_(false) {} 

ProgramThread::~ProgramThread() {
  Release();
}

void ProgramThread::Run(const std::function<GameState*()>& game_state_fn) {
  is_running_ = true;
  thread_.reset(new std::thread([this, game_state_fn]() {
    while(is_running_) {
      ProgramImpl::FromRaw(program_)->Run(game_state_fn());
    }
  }));
}

qbProgram* ProgramThread::Release() {
  if (is_running_) {
    is_running_ = false;
    thread_->join();
    thread_.reset();
  }
  return program_;
}