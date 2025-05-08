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

CoroScheduler::CoroScheduler(qbSchedulerAttr_* attr) {
  size_t num_threads = attr ? attr-> max_async_coros : 16;
  size_t num_small_coros = attr ? attr->max_small_coros : 25;
  size_t num_large_coros = attr ? attr->max_large_coros : 10;

  thread_pool_.reset(new CoroThreadPool(num_threads));
  coros_.reset(new SyncCoros());
  
  small_coros_pool_.reserve(num_small_coros);
  for (size_t i = 0; i < num_small_coros; ++i) {
    qbCoro coro = new qbCoro_{
      .main = coro_new(qbCoroStackSize::QB_CORO_SMALL),
      .is_async = false,
      .ret = qbFuture,
      .arg = qbNil,
      .stack_size = qbCoroStackSize::QB_CORO_SMALL
    };
    small_coros_pool_.push_back(std::move(coro));
    free_small_coros_.push_back(small_coros_pool_.back());
  }

  large_coros_pool_.reserve(num_large_coros);
  for (size_t i = 0; i < num_large_coros; ++i) {
    qbCoro coro = new qbCoro_{
      .main = coro_new(qbCoroStackSize::QB_CORO_LARGE),
      .is_async = true,
      .ret = qbFuture,
      .arg = qbNil,
      .stack_size = qbCoroStackSize::QB_CORO_LARGE
    };
    large_coros_pool_.push_back(std::move(coro));
    free_large_coros_.push_back(large_coros_pool_.back());
  }

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
          coro_init(coro.coro->main, coro.entry);
          coro_state->coros.push_back(coro.coro);
        }
        coro_state->new_coros.resize(0);
      }
      qb_coro_yield(qbNil);
    }

    return qbNil;
  }, QB_CORO_LARGE);
}

qbCoro CoroScheduler::take_coro(qbCoroStackSize stack_size) {
  qbCoro_* ret = nullptr;
  if (stack_size == qbCoroStackSize::QB_CORO_SMALL) {
    std::shared_lock<decltype(free_small_coros_mu_)> l(free_small_coros_mu_);
    if (!free_small_coros_.empty()) {
      ret = free_small_coros_.back();
      free_small_coros_.pop_back();
    }
  } else {
    std::shared_lock<decltype(free_large_coros_mu_)> l(free_large_coros_mu_);
    if (!free_large_coros_.empty()) {
      ret = free_large_coros_.back();
      free_large_coros_.pop_back();
    }
  }

  if (!ret) {
    std::string stack_size_str(stack_size == qbCoroStackSize::QB_CORO_SMALL ?
      "QB_CORO_SMALL" : "QB_CORO_LARGE"
    );
    qb_warn("Out of coroutines for size: %s", stack_size_str.c_str());
  }

  return ret;
}

void CoroScheduler::release_coro(qbCoro coro) {
  coro_clear(coro->main);
  coro->ret = qbFuture;
  coro->arg = qbNil;

  if (coro->stack_size == qbCoroStackSize::QB_CORO_SMALL) {
    std::shared_lock<decltype(free_small_coros_mu_)> l(free_small_coros_mu_);
    free_small_coros_.push_back(coro);
  } else {
    std::shared_lock<decltype(free_large_coros_mu_)> l(free_large_coros_mu_);
    free_large_coros_.push_back(coro);
  }
}

qbCoro CoroScheduler::schedule_defer(qbVar(*entry)(qbVar), qbVar var, qbCoroStackSize stack_size) {
  qbCoro c = take_coro(stack_size);
  if (!c) {
    return nullptr;
  }

  SyncCoro coro{
    .entry = entry,
    .coro = c
  };

  std::lock_guard<decltype(coros_->new_coros_mu)> l(coros_->new_coros_mu);
  coros_->new_coros.push_back(coro);

  return c;
}

qbCoro CoroScheduler::schedule_async(qbVar(*entry)(qbVar), qbVar var, qbCoroStackSize stack_size) {
  qbCoro_* user_coro = take_coro(stack_size);
  if (!user_coro) {
    return nullptr;
  }

  user_coro->ret = qbFuture;
  user_coro->is_async = true;

  thread_pool_->enqueue([this, user_coro, entry, stack_size] (qbVar var) {
    coro_init(user_coro->main, entry);
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

qbVar CoroScheduler::await(qbCoro* pcoro) {
  qbCoro coro = *pcoro;
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
  release_coro(coro);
  *pcoro = nullptr;
  return ret;
}

qbVar CoroScheduler::peek(qbCoro coro) {
  std::shared_lock<decltype(coro->ret_mu)> l(coro->ret_mu);
  return coro->ret;
}

void CoroScheduler::run_sync() {
  qb_coro_call(sync_coro_, qbPtr(coros_.get()));
}