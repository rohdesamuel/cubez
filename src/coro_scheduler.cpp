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

#include "coro_scheduler.h"
#include "defs.h"

#include <shared_mutex>

// Potential optimizations:
//  * make coroutines and stacks an object pool
//  * if there are performance issues with copying large stacks, maybe put the
//    sync_coro into its thread.

CoroScheduler::CoroScheduler(size_t num_threads) {
  thread_pool_.reset(new ThreadPool(num_threads));
  coros_ = new SyncCoros();

  sync_coro_ = qb_coro_create([](qbVar var) {
    SyncCoros* coro_state = (SyncCoros*)var.p;

    for (;;) {
      for (qbCoro coro : coro_state->coros) {
        qbVar ret = qb_coro_call(coro, coro->arg);

        if (coro_done(coro->main)) {
          std::unique_lock<decltype(coro->ret_mu)> l;
          coro->ret = ret;
          coro_state->delete_coros.push_back(coro);
        }
      }

      for (qbCoro coro : coro_state->delete_coros) {
        coro_state->coros.erase(
          std::find(coro_state->coros.begin(), coro_state->coros.end(), coro));
      }
      coro_state->delete_coros.resize(0);

      {
        std::lock_guard<decltype(coro_state->new_coros_mu)> l(coro_state->new_coros_mu);
        for (SyncCoro& coro : coro_state->new_coros) {
          coro.coro->main = coro_new(coro.entry);
          coro_state->coros.push_back(coro.coro);
        }
        coro_state->new_coros.resize(0);
      }
      qb_coro_yield(qbNone);
    }

    return qbNone;
  });
}

CoroScheduler::~CoroScheduler() {
  delete coros_;
}

qbCoro CoroScheduler::schedule_sync(qbVar(*entry)(qbVar), qbVar var) {
  SyncCoro coro;
  coro.entry = entry;

  qbCoro user_coro = new qbCoro_;
  user_coro->main = nullptr;
  user_coro->ret = qbFuture;
  user_coro->arg = var;
  user_coro->is_async = true;

  coro.coro = user_coro;

  std::lock_guard<decltype(coros_->new_coros_mu)> l(coros_->new_coros_mu);
  coros_->new_coros.push_back(coro);

  return user_coro;
}

qbCoro CoroScheduler::schedule_async(qbVar(*entry)(qbVar), qbVar var) {
  qbCoro user_coro = new qbCoro_;
  user_coro->ret = qbFuture;
  user_coro->is_async = true;

  thread_pool_->enqueue([user_coro, entry] (qbVar var) {
    user_coro->main = coro_new(entry);
    int is_done = false;
    qbVar ret = qbFuture;
    do {
      ret = qb_coro_call(user_coro, var);
      is_done = coro_done(user_coro->main);
    } while (!is_done);

    std::unique_lock<decltype(user_coro->ret_mu)> l;
    user_coro->ret = ret;
  }, var);

  return user_coro;
}

qbVar CoroScheduler::await(qbCoro coro) {
  qbVar ret = qbFuture;
  while ((ret = peek(coro)).tag == QB_TAG_NIL) {
    if (coro->is_async) {
      qb_coro_yield(qbFuture);
    } else if (!coro_done(coro->main)) {
      // Calling coro_done is thread-safe with a synchronous coroutine because
      // it is defined to be on the same thread.
      ret = qb_coro_call(coro, coro->arg);
      qb_coro_yield(qbFuture);
    }
  }
  return ret;
}

qbVar CoroScheduler::peek(qbCoro coro) {
  std::shared_lock<decltype(coro->ret_mu)> l(coro->ret_mu);
  return coro->ret;
}

void CoroScheduler::run_sync() {
  qb_coro_call(sync_coro_, qbPtr(coros_));
}