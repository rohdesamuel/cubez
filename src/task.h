#ifndef TASK__H
#define TASK__H

#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include "game_state.h"

class Task {
public:
  Task(qbProgram* program);
  ~Task();

  void Ready();

  void Run(GameState* game_state);

  void Done();

  void Stop();

  void Wait();

private:
  typedef enum {
    STOPPED,
    WAITING,
  } State;

  std::mutex state_lock_;
  std::condition_variable should_run_;
  std::condition_variable should_wait_;

  State state;
  std::atomic_bool stop_;

  std::thread* thread_;
  GameState* game_state_;
  qbProgram* task_;
};

#endif  // TASK__H