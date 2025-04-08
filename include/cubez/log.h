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

#ifndef CUBEZ_LOG__H
#define CUBEZ_LOG__H

#include <cubez/cubez.h>

typedef enum {
  QB_DEBUG = -1,
  QB_INFO = 0,
  QB_WARN = 1,
  QB_ERR = 2,
} qbLogLevel;

QB_API extern const char* QB_STD_OUT;
QB_API extern const char* QB_STD_ERR;

#define __QB_WIDEN2__(x) u8 ## x
#define __QB_WIDEN__(x) __QB_WIDEN2__(x)
#define __QB_FILE_U8__ __QB_WIDEN__(__FILE__)

#define qb_log(level, format, ...) \
  qb_log_ex(level, __QB_FILE_U8__, __LINE__, (utf8_t*)(format), __VA_ARGS__)

#define qb_info(format, ...) \
  qb_log_ex(QB_INFO, __QB_FILE_U8__, __LINE__, (utf8_t*)(format), __VA_ARGS__)

#define qb_warn(format, ...) \
  qb_log_ex(QB_WARN, __QB_FILE_U8__, __LINE__, (utf8_t*)(format), __VA_ARGS__)

#define qb_err(format, ...) \
  qb_log_ex(QB_ERR, __QB_FILE_U8__, __LINE__, (utf8_t*)(format), __VA_ARGS__)

#ifdef __ENGINE_DEBUG__
#ifdef __COMPILE_AS_WINDOWS__
#define qb_fatal(format, ...) \
  do { qb_log_ex(QB_ERR, __QB_FILE_U8__, __LINE__, (utf8_t*)(format), __VA_ARGS__); qb_log_flush(); __debugbreak(); exit(-1); } while(0)
#else
#define qb_fatal(format, ...) \
  do { qb_log_ex(QB_ERR, __QB_FILE_U8__, __LINE__, (utf8_t*)(format), __VA_ARGS__); qb_log_flush(); exit(-1); } while (0)
#endif
#else
#define qb_fatal(format, ...) \
  do { qb_log_ex(QB_ERR, __QB_FILE_U8__, __LINE__, (utf8_t*)(format), __VA_ARGS__); qb_log_flush(); exit(-1); } while (0)
#endif

QB_API void qb_log_ex(qbLogLevel level, const utf8_t* filename, uint64_t fileline, const utf8_t* format, ...);
QB_API void qb_log_flush();

#endif  // CUBEZ_LOG__H
