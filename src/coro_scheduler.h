/**
* Author: Samuel Rohde (rohde.samuel@cubez.io)
*
* Copyright {2020} {Samuel Rohde}
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
  CoroScheduler(size_t num_threads);
  ~CoroScheduler();

  qbCoro schedule_sync(qbVar(*entry)(qbVar), qbVar var);

  // Creates a coroutine and schedules the given function to be run on a
  // background thread. Thread-safe.
  qbCoro schedule_async(qbVar(*entry)(qbVar), qbVar var);

  qbVar await(qbCoro coro);

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

  std::unique_ptr<ThreadPool> thread_pool_;
  SyncCoros* coros_;
  qbCoro sync_coro_;
};

#endif  // CORO_SCHEDULER__H
