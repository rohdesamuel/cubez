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

#ifndef __CORO_H__
#define __CORO_H__

/*
* Portable coroutines for C. Caveats:
*
* 1. You should not take the address of a stack variable, since stack management
*    could reallocate the stack, the new stack would reference a variable in the
*    old stack. Also, cloning a coroutine would cause the cloned coroutine to
*    reference a variable in the other stack.
* 2. You must call coro_init for each kernel thread, since there are thread-local
*    data structures. This will eventually be exploited to scale coroutines across
*    CPUs.
* 3. If setjmp/longjmp inspect the jmp_buf structure before executing a jump, this
*    library probably will not work.
*
* Refs:
* http://www.yl.is.s.u-tokyo.ac.jp/sthreads/
*/

#include "tls.h"
#include <stdlib.h>
#include <stdint.h>
#include <cubez/cubez.h>

/* a coroutine handle */
typedef struct _Coro *Coro;

/* the type of entry function */
typedef qbVar(*_entry)(qbVar var);

/*
* Initialize the coroutine library, returning a coroutine for the thread that called init.
*/
Coro coro_initialize(void* local_sp);

/*
* Create a new coroutine from the given function.
*/
Coro coro_new(qbCoroStackSize stack_size);

/*
* Create a new coroutine from the given function.
*/
void coro_init(Coro c, _entry fn);

Coro coro_clone(Coro target);

// Returns the currently running Coroutine, or NULL if none.
Coro coro_this();

/*
* Invoke a coroutine passing the given value.
*/
qbVar coro_call(Coro target, qbVar var);

/*
* Invoke a coroutine passing the given value.
*/
qbVar coro_yield(qbVar var);

// Returns true if the coroutine is complete.
int coro_done(Coro c);

/*
* Free the coroutine and return the space for the stack.
*/
void coro_free(Coro c);

void coro_clear(Coro c);

// Creates and returns a new coroutine only valid on the current thread.
// Cannot be passed between threads.
qbCoro      qb_coro_create(qbVar(*entry)(qbVar var), qbCoroStackSize stack_size);

// Copies a given coroutine only valid on the current thread. Does not copy the
// coroutine state. Currently, only copies the entry function.
// Cannot be passed between threads.
qbCoro      qb_coro_copy(qbCoro coro);

// A coroutine is safe to destroy only it is finished running. This can be
// queried with qb_coro_peek or qb_coro_done. A coroutine can be waited upon by
// using qb_coro_await.
qbResult    qb_coro_destroy(qbCoro* coro);

// Immediately runs the given coroutine on the same thread as the caller.
// WARNING: A coroutine has its own stack, do not pass in pointers to stack
// variables. They will be invalid pointers.
qbVar       qb_coro_call(qbCoro coro, qbVar var);

#endif /* __CORO_H__ */
