/**
* Author: Samuel Rohde (rohde.samuel@gmail.com)
*
* This file is subject to the terms and conditions defined in
* file 'LICENSE.txt', which is part of this source code package.
*/

#ifndef COMMON__H
#define COMMON__H

#ifndef __LP64__
#define __LP32__
#endif  // __LP_64__

#if ((defined _WIN32 || defined __LP32__) && !defined _WIN64) 
#define __COMPILE_AS_32__
#elif (defined _WIN64 || defined __LP64__)
#define __COMPILE_AS_64__
#endif  // defined _WIN64 || defined __LP64__

#ifdef __ENGINE_DEBUG__
#ifdef __COMPILE_AS_WINDOWS__
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif  // COMPILE_AS_WINDOWS__

#define DEBUG_ASSERT(expr, exit_code) \
do{ if (!(expr)) exit(exit_code); } while (0)

#define DEBUG_OP(expr) do{ expr; } while(0)

#define ASSERT_NOT_NULL(var) \
DEBUG_ASSERT((var) != nullptr, QB_ERROR_NULL_POINTER)

#define ERROR cerr
#define INFO cout
#define LOG(to, str) std::to << str

#else

#define ERROR cerr
#define INFO cout
#define LOG(to, str) do{} while(0)

#define DEBUG_ASSERT(expr, exit_code) do{} while(0)
#define DEBUG_OP(expr) do{} while(0)
#define ASSERT_NOT_NULL(var) do {} while(0)

#endif  // __ENGINE_DEBUG__

#ifdef __cplusplus
#define BEGIN_EXTERN_C extern "C" {
#define END_EXTERN_C }
#include <cstdint>
using std::size_t;
#endif  // ifdef __cplusplus

#if (defined __WIN32__ || defined __CYGWIN32__ || defined _WIN32 || defined _WIN64 || defined _MSC_VER)
#define __COMPILE_AS_WINDOWS__
#elif (defined __linux__ || defined __GNUC__)
#define __COMPILE_AS_LINUX__
#endif

typedef int64_t qbHandle;
typedef int64_t qbOffset;
typedef int64_t qbId;

// Cubez engine success codes.
enum qbResult {
  QB_OK = 0,
  QB_UNKNOWN = 1,
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
  QB_ERROR_EVENTATTR_MESSAGE_SIZE_IS_ZERO = -100,
  QB_ERROR_EVENTATTR_PROGRAM_IS_NOT_SET,
  QB_ERROR_COMPONENTATTR_DATA_SIZE_IS_ZERO = -200,
  QB_ERROR_COMPONENTATTR_PROGRAM_IS_NOT_SET,
  QB_ERROR_ENTITYATTR_COMPONENTS_ARE_EMPTY = -300,
  QB_ERROR_SYSTEMATTR_PROGRAM_IS_NOT_SET = -400,
  QB_ERROR_SYSTEMATTR_HAS_FUNCTION_OR_CALLBACK,
  QB_ERROR_COLLECTIONATTR_PROGRAM_IS_NOT_SET = -500,
  QB_ERROR_COLLECTIONATTR_ACCESSOR_OFFSET_IS_NOT_SET,
  QB_ERROR_COLLECTIONATTR_ACCESSOR_KEY_IS_NOT_SET,
  QB_ERROR_COLLECTIONATTR_ACCESSOR_HANDLE_IS_NOT_SET,
  QB_ERROR_COLLECTIONATTR_KEYITERATOR_DATA_IS_NOT_SET,
  QB_ERROR_COLLECTIONATTR_KEYITERATOR_STRIDE_IS_NOT_SET,
  QB_ERROR_COLLECTIONATTR_VALUEITERATOR_DATA_IS_NOT_SET,
  QB_ERROR_COLLECTIONATTR_VALUEITERATOR_STRIDE_IS_NOT_SET,
  QB_ERROR_COLLECTIONATTR_UPDATE_IS_NOT_SET,
  QB_ERROR_COLLECTIONATTR_INSERT_IS_NOT_SET,
  QB_ERROR_COLLECTIONATTR_COUNT_IS_NOT_SET,
  QB_ERROR_COLLECTIONATTR_IMPLEMENTATION_IS_NOT_SET,
};

#endif
