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

#define qb_log(level, format, ...) \
  qb_log_ex(level, __FILE__, __LINE__, format, __VA_ARGS__)

QB_API void qb_log_ex(qbLogLevel level, const char* filename, uint64_t fileline, const char* format, ...);

#endif  // CUBEZ_LOG__H
