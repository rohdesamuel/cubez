#include "program_thread.h"

#include "program_impl.h"

ProgramThread::ProgramThread(qbProgram* program) :
    program_(program), is_running_(false) {} 

ProgramThread::~ProgramThread() {
  Release();
}

void ProgramThread::Run() {
  is_running_ = true;
  thread_.reset(new std::thread([this]() {
    while(is_running_) {
      ProgramImpl::FromRaw(program_)->Run();
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