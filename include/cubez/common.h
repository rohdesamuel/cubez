/**
* Author: Samuel Rohde (rohde.samuel@cubez.io)
*
* Copyright {2020} {Samuel Rohde}
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

#ifdef __ENGINE_DEBUG__
#ifdef __COMPILE_AS_WINDOWS__
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif  // COMPILE_AS_WINDOWS__

#define DEBUG_ASSERT(expr, exit_code) \
do{ if (!(expr)) { std::cerr << #expr << std::endl; exit(exit_code);} } while (0)

#define DEBUG_OP(expr) do{ expr; } while(0)

#define ASSERT_NOT_NULL(var) \
DEBUG_ASSERT((var) != nullptr, QB_ERROR_NULL_POINTER)

#ifdef __COMPILE_AS_WINDOWS__
#define INFO(x) { std::cerr << "[INFO] " << __FUNCSIG__ << " @ Line " << __LINE__ << ":\n\t" << x << std::endl; }
#define FATAL(x) { std::cerr << "[FATAL] " << __FUNCSIG__ << " @ Line " << __LINE__ << ":\n\t" << x << std::endl; \
  __debugbreak(); std::cin.get(); exit(-1); }
#else
#define INFO(x) { std::cerr << "[INFO] " << __PRETTY_FUNCTION__ << " @ Line " << __LINE__ << ":\n\t" << x << std::endl; }
#define FATAL(x) { std::cerr << "[FATAL] " << __PRETTY_FUNCTION__ << " @ Line " << __LINE__ << ":\n\t" << x << std::endl; \
  std::cin.get(); exit(-1); }
#endif  // __COMPILE_AS_WINDOWS__

#else

#define INFO(x)
#ifdef __COMPILE_AS_WINDOWS__
#define FATAL(x) { std::cerr << "[FATAL] " << __FUNCSIG__ << " @ Line " << __LINE__ << ":\n\t" << x << std::endl; \
  __debugbreak(); std::cin.get(); exit(-1); }
#else
#define FATAL(x) { std::cerr << "[FATAL] " << __PRETTY_FUNCTION__ << " @ Line " << __LINE__ << ":\n\t" << x << std::endl; \
  std::cin.get(); exit(-1); }
#endif  // __COMPILE_AS_WINDOWS__

#define DEBUG_ASSERT(expr, exit_code) do{} while(0)
#define DEBUG_OP(expr) do{} while(0)
#define ASSERT_NOT_NULL(var) do {} while(0)

#endif  // __ENGINE_DEBUG__

#ifdef __cplusplus
#define BEGIN_EXTERN_C extern "C" {
#define END_EXTERN_C }
#include <cstdint>
#else
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
#define CPP_API 
#endif  // _EXPORT_BUILD
#define STRCPY strcpy_s
#define STRDUP _strdup
#define SSCANF sscanf_s
#define ALIGNED_ALLOC _aligned_malloc
#define ALIGNED_FREE _aligned_free
#else
#define API extern "C"
#define STRCPY strcpy
#define STRDUP strdup
#define SSCANF sscanf
#define ALIGNED_ALLOC(size, alignment) aligned_alloc((alignment), (size))
#define ALIGNED_FREE free
#endif

#ifndef __COMPILE_AS_WINDOWS__
using std::size_t;
#endif

typedef int64_t qbHandle;
typedef int64_t qbOffset;
typedef int64_t qbId;

// Cubez engine success codes.
enum qbResult {
  QB_OK = 0,
  QB_UNKNOWN = 1,
  QB_DONE = 2,
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
  QB_ERROR_COLLECTIONATTR_PROGRAM_IS_NOT_SET = -500,
  QB_ERROR_COLLECTIONATTR_ACCESSOR_OFFSET_IS_NOT_SET = -501,
  QB_ERROR_COLLECTIONATTR_ACCESSOR_KEY_IS_NOT_SET = -502,
  QB_ERROR_COLLECTIONATTR_ACCESSOR_HANDLE_IS_NOT_SET = -503,
  QB_ERROR_COLLECTIONATTR_KEYITERATOR_DATA_IS_NOT_SET = -504,
  QB_ERROR_COLLECTIONATTR_KEYITERATOR_STRIDE_IS_NOT_SET = -505,
  QB_ERROR_COLLECTIONATTR_VALUEITERATOR_DATA_IS_NOT_SET = -506,
  QB_ERROR_COLLECTIONATTR_VALUEITERATOR_STRIDE_IS_NOT_SET = -507,
  QB_ERROR_COLLECTIONATTR_UPDATE_IS_NOT_SET = -508,
  QB_ERROR_COLLECTIONATTR_INSERT_IS_NOT_SET = -509,
  QB_ERROR_COLLECTIONATTR_COUNT_IS_NOT_SET = -510,
  QB_ERROR_COLLECTIONATTR_IMPLEMENTATION_IS_NOT_SET = -511,
  QB_ERROR_COLLECTIONATTR_REMOVE_BY_OFFSET_IS_NOT_SET = -512,
  QB_ERROR_COLLECTIONATTR_REMOVE_BY_KEY_IS_NOT_SET = -513,
  QB_ERROR_COLLECTIONATTR_REMOVE_BY_HANDLE_IS_NOT_SET = -514,
};

#endif  // CUBEZ_COMMON__H
