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

#ifndef __CTXT_H__
#define __CTXT_H__

#include <cubez/common.h>
#include <setjmp.h>
#include <stdint.h>

#if defined(__clang__)
#pragma clang optimize off
#elif defined(__GNUC__) || defined(__GNUG__)
#pragma GCC push_options
#pragma GCC optimize ("O0")
#elif defined(_MSC_VER)
#pragma optimize( "", off )
#endif

/* context management definitions */
typedef jmp_buf _ctxt;

#ifndef _setjmp
#define _setjmp setjmp
#endif

#ifndef _longjmp
#define _longjmp longjmp
#endif

#define _save_and_resumed(c) _setjmp(c)
#define _rstr_and_jmp(c) _longjmp(c, 1)

/* true if stack grows up, false if down */
static int _stack_grows_up;

static void _infer_direction_from(int *first_addr) {
  int second;
  _stack_grows_up = (first_addr < &second);
}

static void _infer_stack_direction() {
  int first;
  _infer_direction_from(&first);
}

#if defined(__clang__)
#pragma clang optimize on
#elif defined(__GNUC__) || defined(__GNUG__)
#pragma GCC pop_options
#elif defined(_MSC_VER)
#pragma optimize( "", on )
#endif

#endif /*__CTXT_H__*/
