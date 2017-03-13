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
DEBUG_ASSERT((var) != nullptr, ::cubez::Status::Code::NULL_POINTER)

#else
#define DEBUG_ASSERT(expr, exit_code) do{} while(0)
#define DEBUG_OP(expr) do{} while(0)
#define ASSERT_NOT_NULL(var) do {} while(0)

#endif  // __ENGINE_DEBUG__

#ifdef __cplusplus
#define BEGIN_EXTERN_C extern "C" {
#define END_EXTERN_C }
#endif  // ifdef __cplusplus

#include <cstdint>
using std::size_t;

namespace cubez
{

#if (defined __WIN32__ || defined __CYGWIN32__ || defined _WIN32 || defined _WIN64 || defined _MSC_VER)
#define __COMPILE_AS_WINDOWS__
#elif (defined __linux__ || defined __GNUC__)
#define __COMPILE_AS_LINUX__
#endif

#define CACHE_LINE_SIZE 64
#ifdef __COMPILE_AS_LINUX__
#define __CACHE_ALIGNED__ __attribute__((aligned(64)))

template<typename T>
struct __CACHE_ALIGNED__ CacheAlligned {
  T data;
};
#elif defined __COMPILE_AS_WINDOWS__
#define __CACHE_ALIGNED__ __declspec(align(CACHE_LINE_SIZE))
template<typename T>
struct __CACHE_ALIGNED__ CacheAlligned {
  T data;
};
#endif

typedef int64_t Handle;
typedef int64_t Offset;
typedef int64_t Id;

struct Status {
  enum Code {
    UNKNOWN = -1,
    OK = 0,
    MEMORY_LEAK,
    MEMORY_OUT_OF_BOUNDS,
    NULL_POINTER,
    UNKNOWN_INDEXED_BY_VALUE,
    INCOMPATIBLE_DATA_TYPES,
    FAILED_INITIALIZATION,
    BAD_RUN_STATE,
    DOES_NOT_EXIST,
    ALREADY_EXISTS,
    UNKNOWN_TRIGGER_POLICY,
  };

  Status(Code code=Code::OK, const char* message=""):
      code(code),
      message(message) { }


  Code code;
  const char* message;

  operator bool() {
    return code == Code::OK;
  }
};

}  // namespace cubez

#endif
