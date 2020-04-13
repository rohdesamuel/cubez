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
Coro coro_new(_entry fn);

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

#endif /* __CORO_H__ */
