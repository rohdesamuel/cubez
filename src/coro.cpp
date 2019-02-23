/*
* Co-routines in C
*
* Possible implementations:
* 1. stack switching - need to constantly check stack usage (I use this).
* 2. stack copying - essentially continuations.
*
* Notes:
* * termination of a coroutine without explicit control transfer returns control
*   to the coroutine which initialized the coro library.
*
* Todo:
* 1. Co-routines must be integrated with any VProc/kernel-thread interface, since
*    an invoked co-routine might be running on another cpu. A coro invoker must
*    check that the target vproc is the same as the current vproc; if not, queue the
*    invoker on the target vproc using an atomic op.
* 2. VCpu should implement work-stealing, ie. when its run queue is exhausted, it
*    should contact another VCpu and steal a few of its coros, after checking its
*    migration queues of course. The rate of stealing should be tuned:
*    http://www.cs.cmu.edu/~acw/15740/proposal.html
* 3. Provide an interface to register a coroutine for any errors generated. This is
*    a type of general Keeper, or exception handling.
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "coro.h"
#include "tls.h"
#include "ctxt.h"
#include <cubez/cubez.h>
#include <cubez/common.h>

/*
* These are thresholds used to grow and shrink the stack. They are scaled by
* the size of various platform constants. STACK_TGROW is sized to allow
* approximately 200 nested calls. On a 32-bit machine assuming 4 words of
* overhead per call, 256 calls = 1024. If stack allocation is performed,
* this will need to be increased.
*/
#define STACK_TGROW (1 << 10)
#define STACK_DEFAULT sizeof(intptr_t) * STACK_TGROW
#define STACK_TSHRINK 2 * STACK_DEFAULT
#define STACK_ADJ STACK_DEFAULT

/* the coroutine structure */
struct _Coro {
  Coro parent;
  _ctxt ctxt;
  _entry start;
  void* stack_base;
  size_t stack_size;
  size_t stack_used;
  int is_done;
};

/*
* Each of these are local to the kernel thread. Volatile storage is necessary
* otherwise _value is often cached as a local when switching contexts, so
* a sequence of calls will always return the first value!
*/
THREAD_LOCAL volatile Coro _cur;
THREAD_LOCAL volatile qbVar _value;
THREAD_LOCAL struct _Coro _on_exit;
THREAD_LOCAL uintptr_t _sp_base;

void _fix_frame_pointer(jmp_buf buf) {
#if defined(__COMPILE_AS_WINDOWS__) && defined(__COMPILE_AS_64__)
  // See https://stackoverflow.com/questions/26605063/an-invalid-or-unaligned-stack-was-encountered-during-an-unwind-operation.
  ((_JUMP_BUFFER*)buf)->Frame = 0;
#endif
}

void _coro_save(Coro to, uintptr_t mark) {
  uintptr_t sp = (_stack_grows_up ? _sp_base : mark);
  size_t sz = (_stack_grows_up ? mark - _sp_base : _sp_base - mark);
  if (to->stack_size < sz + STACK_TGROW || to->stack_size > sz - STACK_TSHRINK) {
    size_t newsz = sz + STACK_ADJ;
    free(to->stack_base);
    to->stack_base = calloc(1, newsz);
    to->stack_size = newsz;
  }
  to->stack_used = sz;
  memcpy(to->stack_base, (void *)sp, sz);
}

void _coro_restore(uintptr_t target_sz, void ** pad) {
  if ((uintptr_t)&pad - _sp_base > target_sz) {
    size_t sz = _cur->stack_used;
    void * sp = (void *)(_stack_grows_up ? _sp_base : _sp_base - sz);
    memcpy(sp, _cur->stack_base, sz);
    _fix_frame_pointer(_cur->ctxt);
    _rstr_and_jmp(_cur->ctxt);
  } else {
    /* recurse until the stack depth is greater than target stack depth */
    void* padding[STACK_TGROW];
    _coro_restore(target_sz, padding);
  }
}

qbVar _coro_fastcall(Coro target, qbVar value) {
#ifdef __COMPILE_AS_64__
  char stack_top[16];
#elif defined (__COMPILE_AS_32__) 
  char stack_top[8];
#endif
  *(qbVar*)(&_value) = value; /* pass value to 'target' */
  if (!_save_and_resumed(_cur->ctxt)) {
    /* _sp_base - &local is used to calculate the size of the env */
    uintptr_t target_sz = (_stack_grows_up
                           ? 0
                           : target->stack_size);
    _coro_save(_cur, (uintptr_t)&stack_top);
    _cur = target;
    _coro_restore(target_sz, NULL);
  }
  /* when someone called us, just return the value */
  return *(qbVar*)(&_value);
}

/*
* This function invokes the start function of the coroutine when the
* coroutine is first called. If it was called from coro_new, then it sets
* up the stack and initializes the saved context.
*/
void _coro_enter(Coro c) {
  if (_save_and_resumed(c->ctxt)) {       /* start the coroutine; stack is empty at this point. */
    qbVar _return;
    _return.p = _cur;
    _cur->start(*(qbVar*)(&_value));
    _cur->is_done = 1;
    /* return the exited coroutine to the exit handler */
    _coro_fastcall(&_on_exit, _return);
  } else {
    void* stack_top;
    _coro_save(c, (intptr_t)&stack_top);
  }
}

void _stack_init(Coro c, size_t init_size) {
  c->stack_size = init_size;
  c->stack_base = calloc(1, c->stack_size);
}

#if defined(__clang__)
#pragma clang optimize off
#elif defined(__GNUC__) || defined(__GNUG__)
#pragma GCC push_options
#pragma GCC optimize ("O0")
#elif defined(_MSC_VER)
#pragma optimize( "", off )
#endif

/*
* We probe the current machine and extract the data needed to modify the
* machine context. The current thread is then initialized as the currently
* executing coroutine.
*/
Coro coro_initialize(void* sp_base) {
  _infer_stack_direction();
  _sp_base = (intptr_t)sp_base;
  _stack_init(&_on_exit, STACK_DEFAULT);

  _cur = &_on_exit;
  _coro_enter(&_on_exit);
  return _cur;
}

#if defined(__clang__)
#pragma clang optimize on
#elif defined(__GNUC__) || defined(__GNUG__)
#pragma GCC pop_options
#elif defined(_MSC_VER)
#pragma optimize( "", on )
#endif

Coro coro_new(_entry fn) {
  Coro c = (Coro)malloc(sizeof(struct _Coro));
  _stack_init(c, STACK_DEFAULT);

  c->start = fn;
  c->is_done = 0;
  _coro_enter(c);
  return c;
}

Coro coro_this() {
  return _cur == &_on_exit ? nullptr : _cur;
}

int coro_done(Coro c) {
  return c == &_on_exit ? 0 : c->is_done;
}

/*
* First, set the value in the volatile global. If _value were not volatile, this value
* would be cached on the stack, and hence saved and restored on every call. We then
* save the context for the current coroutine, set the target coroutine as the current
* one running, then restore it. The target is now running, and it simply returns _value.
*/
qbVar coro_call(Coro target, qbVar value) {
#ifdef __COMPILE_AS_64__
  char stack_top[16];
#elif defined (__COMPILE_AS_32__) 
  char stack_top[8];
#endif
  *(qbVar*)(&_value) = value; /* pass value to 'target' */
  if (!_save_and_resumed(_cur->ctxt)) {
    /* _sp_base - &local is used to calculate the size of the env */
    uintptr_t target_sz = (_stack_grows_up
                           ? 0
                           : target->stack_size);
    _coro_save(_cur, (uintptr_t)&stack_top);
    target->parent = _cur;
    _cur = target;
    _coro_restore(target_sz, NULL);
  }
  /* when someone called us, just return the value */
  return *(qbVar*)(&_value);
}

qbVar coro_yield(qbVar var) {
  if (_cur->parent) {
    return _coro_fastcall(_cur->parent, var);
  }
  return qbNone;
}

void coro_free(Coro c) {
  if (c->stack_base != NULL) {
    free((void *)c->stack_base);
  }
  free(c);
}