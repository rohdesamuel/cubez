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

#ifndef CUBEZ_COMMON__H
#define CUBEZ_COMMON__H

#ifndef __LP64__
#define __LP32__
#endif  // __LP_64__

#if ((defined _WIN32 || defined __LP32__) && !defined _WIN64) 
#define __COMPILE_AS_32__
#elif (defined _WIN64 || defined __LP64__)
#define __COMPILE_AS_64__
#endif  // defined _WIN64 || defined __LP64__

#if (defined __WIN32__ || defined __CYGWIN32__ || defined _WIN32 || defined _WIN64 || defined _MSC_VER)
#define __COMPILE_AS_WINDOWS__
#elif (defined __linux__ || defined __GNUC__)
#define __COMPILE_AS_LINUX__
#endif

#define QB_ASSERT(expr) \
do{ if (!(expr)) { qb_fatal_ex(QB_PANIC, "Failed assertion: "#expr);} } while (0)

#define QB_ASSERT_OK(expr) \
do{ if ((expr) != QB_OK) { qb_fatal_ex(QB_PANIC, "Failed assertion: "#expr);} } while (0)

#ifdef __ENGINE_DEBUG__
#include <iostream>

#define DEBUG_ASSERT(expr, exit_code) \
do{ if (!(expr)) { qb_fatal_ex(exit_code, "Failed assertion: "#expr);} } while (0)

#define DEBUG_OP(expr) do{ expr; } while(0)

#define ASSERT_NOT_NULL(var) \
DEBUG_ASSERT((var) != nullptr, QB_ERROR_NULL_POINTER)

#else

#define DEBUG_ASSERT(expr, exit_code) do{} while(0)
#define DEBUG_OP(expr) do{} while(0)
#define ASSERT_NOT_NULL(var) do {} while(0)

#endif  // __ENGINE_DEBUG__


#ifdef __cplusplus
#define BEGIN_EXTERN_C extern "C" {
#define END_EXTERN_C }
#include <cstdint>
#include <cassert>
#else
#include <stdint.h>
#include <assert.h>
#ifndef bool
#define bool int
#define true 1
#define false 0
#endif  // ifndef bool
#endif  // ifdef __cplusplus

#ifdef __COMPILE_AS_WINDOWS__
#ifdef __BUILDING_DLL__
#define QB_API extern "C" __declspec(dllexport)
#else 
#define QB_API extern "C" __declspec(dllimport)
#endif  // _EXPORT_BUILD
#define STRCPY strcpy_s
#define STRDUP _strdup
#define SSCANF sscanf_s
#define ALIGNED_ALLOC(size, alignment) _aligned_malloc((size), (alignment))
#define ALIGNED_FREE(ptr) _aligned_free(ptr)
#define QB_PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop))
#else
#define QB_API extern "C"
#define STRCPY strcpy
#define STRDUP strdup
#define SSCANF sscanf
#define ALIGNED_ALLOC(size, alignment) aligned_alloc((alignment), (size))
#define ALIGNED_FREE(ptr) free(ptr)
#define QB_PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
#endif

typedef int qbBool;
#define QB_TRUE 1
#define QB_FALSE 0

typedef int64_t qbOffset;
typedef int64_t qbId;

#ifdef __cplusplus
typedef char8_t utf8_t;
#ifndef __COMPILE_AS_WINDOWS__
using std::size_t;
#endif
#else
typedef uint8_t utf8_t;
#endif

// A handle is an id that refers to a piece of memory owned by the engine. The
// data refered to by the id may be destroyed while the user still has the
// handle.
typedef qbId qbHandle;

QB_API extern const qbHandle qbInvalidHandle;

#define QB_MAX_NAME_LENGTH 128

// Cubez engine success codes.
enum qbResult {
  QB_OK = 0,
  QB_UNKNOWN,
  QB_PANIC,
  QB_DONE,
  QB_EOF,

  QB_ERROR_MEMORY_LEAK = -1,
  QB_ERROR_MEMORY_OUT_OF_BOUNDS = -2,
  QB_ERROR_OUT_OF_MEMORY = -3,
  QB_ERROR_NULL_POINTER = -4,
  QB_ERROR_UNKNOWN_INDEXED_BY_VALUE = -5,
  QB_ERROR_INCOMPATIBLE_DATA_TYPES = -6,
  QB_ERROR_FAILED_INITIALIZATION = -7,
  QB_ERROR_BAD_RUN_STATE = -8,
  QB_ERROR_DOES_NOT_EXIST = -9,
  QB_ERROR_ALREADY_EXISTS = -10,
  QB_ERROR_UNKNOWN_TRIGGER_POLICY = -11,
  QB_ERROR_NOT_FOUND = -12,
  QB_ERROR_PROGRAM_ALREADY_JOINED = -13,
  QB_ERROR_MAX_COMPONENT_COUNT_REACHED = -14,
  QB_ERROR_EVENTATTR_MESSAGE_SIZE_IS_ZERO = -100,
  QB_ERROR_EVENTATTR_PROGRAM_IS_NOT_SET = -101,
  QB_ERROR_COMPONENTATTR_DATA_SIZE_IS_ZERO = -200,
  QB_ERROR_COMPONENTATTR_PROGRAM_IS_NOT_SET = -201,
  QB_ERROR_ENTITYATTR_COMPONENTS_ARE_EMPTY = -300,
  QB_ERROR_SYSTEMATTR_PROGRAM_IS_NOT_SET = -400,
  QB_ERROR_SYSTEMATTR_HAS_FUNCTION_OR_CALLBACK = -401,

  QB_ERROR_SEMAPHORE_NONMONOTONIC_SIGNAL = -500,
  QB_ERROR_SOCKET = -600,
  QB_ERROR_MAX_TABLES_REACHED = -700,

  QB_ERROR_FILE_NOT_FOUND = -800,
  QB_ERROR_FILE_ALREADY_EXISTS = -801,
  QB_ERROR_FILE_IS_A_DIRECTORY = -802,
  QB_ERROR_FILE_TOO_MANY_OPEN_FILES = -803,
  QB_ERROR_FILE_INVALID_SEEK = -804,
  QB_ERROR_FILE_PERMISSION_DENIED = -805,
};

#endif  // CUBEZ_COMMON__H
