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
#define STACK_TGROW (1 << 20)
#define STACK_DEFAULT sizeof(intptr_t) * STACK_TGROW
#define STACK_TSHRINK 2 * STACK_DEFAULT
#define STACK_ADJ STACK_DEFAULT

/* the coroutine structure */
struct _Coro {
  Coro parent;
  _ctxt ctxt;
  _entry start;
  intptr_t stack_base;
  size_t stack_size;
};

/*
* Each of these are local to the kernel thread. Volatile storage is necessary
* otherwise _value is often cached as a local when switching contexts, so
* a sequence of calls will always return the first value!
*/
THREAD_LOCAL volatile Coro _cur;
THREAD_LOCAL volatile qbVar _value;
THREAD_LOCAL struct _Coro _on_exit;

/*EXPORT
coro coro_error()
{
coro c = (coro)malloc(sizeof(struct _coro));
c->stack_base = NULL;
c->stack_size = 0;
c->start = NULL;
if (!_save_and_resumed(c->ctxt))
{
_cur = c;
}
return _cur;
}*/

#pragma optimize( "", off )

void _fix_frame_pointer(jmp_buf buf) {
#if defined(__COMPILE_AS_WINDOWS__) && defined(__COMPILE_AS_64__)
  // See https://stackoverflow.com/questions/26605063/an-invalid-or-unaligned-stack-was-encountered-during-an-unwind-operation.
  ((_JUMP_BUFFER*)buf)->Frame = 0;
#endif
}

/* copy the old stack frame to the new stack frame */
void _coro_cpframe(intptr_t local_sp, intptr_t new_sp) {
  intptr_t src = local_sp - (_stack_grows_up ? _frame_offset : 0);
  intptr_t dst = new_sp - (_stack_grows_up ? _frame_offset : 0);
  /* copy local stack frame to the new stack */
  memcpy((void *)dst, (void *)src, _frame_offset);
}

/* rebase any values in saved state to the new stack */
void _coro_rebase(Coro c, intptr_t local_sp, intptr_t new_sp) {
  intptr_t * s = (intptr_t *)c->ctxt;
  ptrdiff_t diff = new_sp - local_sp; /* subtract old base, and add new base */
  int i;
  for (i = 0; i < _offsets_len; ++i) {
    s[_offsets[i]] = new_sp; //+= diff;
  }
}

qbVar _coro_fastcall(Coro target, qbVar value) {
  /* FIXME: ensure target is on the same proc as cur, else, migrate cur to target->proc */

  *(qbVar*)(&_value) = value; /* pass value to 'target' */
  if (!_save_and_resumed(_cur->ctxt)) {
    /* we are calling someone else, so we set up the environment, and jump to target */
    _cur = target;
    _fix_frame_pointer(_cur->ctxt);
    _rstr_and_jmp(_cur->ctxt);
  }
  /* when someone called us, just return the value */
  return *(qbVar*)(&_value);
}

void _coro_initstack(Coro c) {
  /* local and new stack pointers at identical relative positions on the stack */
  intptr_t local_sp = (intptr_t)&local_sp;

  /* I don't know what the addition "- sizeof(void *)" is for when
  the stack grows downards */
  intptr_t new_sp = c->stack_base +
    (_stack_grows_up
     ? _frame_offset
     : c->stack_size - _frame_offset - sizeof(void *));

  // Align the stack to 16 bytes.
  new_sp &= ~(uintptr_t)0x0F;

  /* copy local stack frame to the new stack */
  _coro_cpframe(local_sp, new_sp);

  /* reset any locals in the saved state to point to the new stack */
  _coro_rebase(c, local_sp, new_sp);
}

/*
* This function invokes the start function of the coroutine when the
* coroutine is first called. If it was called from coro_new, then it sets
* up the stack and initializes the saved context.
*/
void _coro_enter(Coro c) {
  if (_save_and_resumed(c->ctxt)) {	/* start the coroutine; stack is empty at this point. */
    qbVar _return;
    _return.p = _cur;
    _return = _cur->start(*(qbVar*)(&_value));
    /* return the exited coroutine to the exit handler */
    _coro_fastcall(_cur->parent, _return);
  }
  /* this code executes when _coro_enter is called from coro_new */
INIT_CTXT:
  _coro_initstack(c);
}

/*
* We probe the current machine and extract the data needed to modify the
* machine context. The current thread is then initialized as the currently
* executing coroutine.
*/
Coro coro_init() {
  _probe_arch();
  _cur = &_on_exit;
  memset(&_on_exit, 0, sizeof(_on_exit));
  return _cur;
}

Coro coro_new(_entry fn) {
  /* FIXME: should not malloc directly? */
  Coro c = (Coro)malloc(sizeof(struct _Coro));
  memset((void*)c, 0, sizeof(struct _Coro));

  c->stack_size = STACK_DEFAULT;
  c->stack_base = (intptr_t)malloc(c->stack_size);// _aligned_malloc(c->stack_size, 16);// ;
  memset((void*)c->stack_base, 0, c->stack_size);
  c->start = fn;

  _coro_enter(c);
  return c;
}

/*
* First, set the value in the volatile global. If _value were not volatile, this value
* would be cached on the stack, and hence saved and restored on every call. We then
* save the context for the current coroutine, set the target coroutine as the current
* one running, then restore it. The target is now running, and it simply returns _value.
*/
qbVar coro_call(Coro target, qbVar value) {
  /* FIXME: ensure target is on the same proc as cur, else, migrate cur to target->proc */

  *(qbVar*)(&_value) = value; /* pass value to 'target' */
  if (!_save_and_resumed(_cur->ctxt)) {
    /* we are calling someone else, so we set up the environment, and jump to target */
    target->parent = _cur;
    _cur = target;
    _fix_frame_pointer(_cur->ctxt);
    _rstr_and_jmp(_cur->ctxt);
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

Coro coro_clone(Coro c) {
  Coro cnew = (Coro)malloc(sizeof(struct _Coro));
  size_t stack_sz = c->stack_size;
  intptr_t stack_base = (intptr_t)malloc(stack_sz);
  /* copy the context then the stack data */
  memcpy(cnew, c, sizeof(struct _Coro));
  memcpy((void *)stack_base, (void *)c->stack_base, stack_sz);
  cnew->stack_base = stack_base;
  cnew->stack_size = stack_sz;
  /* ensure new context references new stack */
  _coro_rebase(cnew, c->stack_base, stack_base);
  return cnew;
}

void coro_free(Coro c) {
  free((void *)c->stack_base);
  free(c);
}

/*
* Resume execution with a new stack:
*  1. allocate a new stack
*  2. copy all relevant data
*  3. mark save point
*  4. rebase the context using the new stack
*  5. restore the context with the new stack
*/
static void _coro_resume_with(size_t sz) {
  /* allocate bigger stack */
  intptr_t old_sp = _cur->stack_base;
  void * new_sp = malloc(sz);
  memcpy(new_sp, (void *)old_sp, _cur->stack_size);
  _cur->stack_base = (intptr_t)new_sp;
  _cur->stack_size = sz;
  /* save the current context; execution resumes here with new stack */
  if (!_save_and_resumed(_cur->ctxt)) {
    /* rebase jmp_buf using new stack */
    _coro_rebase(_cur, old_sp, (intptr_t)new_sp);
    _fix_frame_pointer(_cur->ctxt);
    _rstr_and_jmp(_cur->ctxt);
  }
  free((void *)old_sp);
}

/*
* The stack poll uses some hysteresis to avoid thrashing. We grow the stack if
* there's less than STACK_TGROW bytes left in the current stack, and we only shrink
* if there's more than STACK_TSHRINK empty.
*/
void coro_poll() {
  /* check the current stack pointer */
  size_t stack_size = _cur->stack_size;
  size_t empty = (_stack_grows_up
                  ? stack_size - ((uintptr_t)&empty - _cur->stack_base)
                  : (uintptr_t)&empty - _cur->stack_base);

  if (empty < STACK_TGROW) {	/* grow stack */
    _coro_resume_with(stack_size + STACK_ADJ);
  } else if (empty > STACK_TSHRINK) {	/* shrink stack */
    _coro_resume_with(stack_size - STACK_ADJ);
  }
}

#pragma optimize( "", on )