/**
* Author: Samuel Rohde (rohde.samuel@cubez.io)
*
* Copyright 2020 Samuel Rohde
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#ifndef CORO_SCHEDULER__H
#define CORO_SCHEDULER__H

#include <cubez/cubez.h>

#include "thread_pool.h"

class CoroScheduler {
public:
  CoroScheduler(qbSchedulerAttr_* attr);
  ~CoroScheduler() = default;

  qbCoro schedule_defer(qbVar(*entry)(qbVar), qbVar var, qbCoroStackSize stack_size);

  // Creates a coroutine and schedules the given function to be run on a
  // background thread. Thread-safe.
  qbCoro schedule_async(qbVar(*entry)(qbVar), qbVar var, qbCoroStackSize stack_size);

  qbVar await(qbCoro* coro);

  qbVar peek(qbCoro coro);

  void run_sync();

private:
  struct SyncCoro {
    qbVar(*entry)(qbVar);
    qbCoro coro;
  };

  struct SyncCoros {
    std::vector<qbCoro> coros;
    std::vector<qbCoro> delete_coros;

    std::mutex new_coros_mu;
    std::vector<SyncCoro> new_coros;
  };

  qbCoro take_coro(qbCoroStackSize stack_size);
  void release_coro(qbCoro coro);

  std::unique_ptr<CoroThreadPool> thread_pool_;
  std::unique_ptr<SyncCoros> coros_;
  qbCoro sync_coro_;

  std::shared_mutex free_small_coros_mu_;
  std::vector<qbCoro> free_small_coros_;

  std::shared_mutex free_large_coros_mu_;
  std::vector<qbCoro> free_large_coros_;

  std::vector<qbCoro> small_coros_pool_;
  std::vector<qbCoro> large_coros_pool_;
};

#endif  // CORO_SCHEDULER__H
