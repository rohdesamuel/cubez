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

#include <cubez/utils.h>
#include "utils_internal.h"

#include <algorithm>
#include <vector>

#ifdef __COMPILE_AS_WINDOWS__
#define _WINSOCKAPI_
#include <Windows.h>
#undef min
#undef max
#elif defined (__COMPILE_AS_LINUX__)
#include <time.h>
#endif

typedef struct qbTimer_ {
#ifdef __COMPILE_AS_WINDOWS__
  // Units of count.
  uint64_t start_;

  // Units of count.
  uint64_t end_;

#elif defined (__COMPILE_AS_LINUX__)
  timespec start_;
  timespec end_;
#endif

  uint8_t iterator_;
  uint8_t window_size_;

  // Contains elapsed counts between start and when the value was added.
  std::vector<int64_t> window_;
} qbTimer_;

// 1e9 converts to ns
// 1e6 converts to us
const int64_t kTimeUnits = (int64_t)1000000000;
uint64_t kStartTime = 0;

#ifdef __COMPILE_AS_WINDOWS__
int64_t kClockFrequency = 0;
#elif defined(__COMPILE_AS_LINUX__)
#define CLOCK_TYPE CLOCK_MONOTONIC //CLOCK_PROCESS_CPUTIME_ID

timespec diff(timespec start, timespec end) {
  timespec temp;
  if ((end.tv_nsec - start.tv_nsec)<0) {
    temp.tv_sec = end.tv_sec - start.tv_sec - 1;
    temp.tv_nsec = NS_TO_SEC + end.tv_nsec - start.tv_nsec;
  } else {
    temp.tv_sec = end.tv_sec - start.tv_sec;
    temp.tv_nsec = end.tv_nsec - start.tv_nsec;
  }
  return temp;
}
#endif  // ifdef __COMPILE_AS_LINUX__

void utils_initialize() {
  LARGE_INTEGER freq;
  QueryPerformanceFrequency(&freq);
  kClockFrequency = freq.QuadPart;
  
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  kStartTime = now.QuadPart;
}

uint64_t convert_to_time(uint64_t count) {
  return count * kTimeUnits / kClockFrequency;
}

int64_t qb_timer_query() {
  int64_t ret = 0;
#ifdef __COMPILE_AS_WINDOWS__
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  ret = convert_to_time(now.QuadPart - kStartTime);
#elif defined (__COMPILE_AS_LINUX__)
  timespec now;
  clock_gettime(CLOCK_TYPE, &now);
  ret = (NS_TO_SEC * now.tv_sec) + now.tv_nsec;
#endif
  return ret;
}

int64_t qb_timer_now() {
  int64_t ret = 0;
#ifdef __COMPILE_AS_WINDOWS__
  LARGE_INTEGER now;
  QueryPerformanceCounter(&now);
  ret = now.QuadPart - kStartTime;
#elif defined (__COMPILE_AS_LINUX__)
  timespec now;
  clock_gettime(CLOCK_TYPE, &now);
  ret = (NS_TO_SEC * now.tv_sec) + now.tv_nsec;
#endif
  return ret;
}

qbResult qb_timer_create(qbTimer* timer, uint8_t window_size) {
  qbTimer_* result = new qbTimer_;
  result->window_size_ = window_size;
  result->window_.resize(window_size);

  qb_timer_reset(result);
  *timer = result;
  return QB_OK;
}

qbResult qb_timer_destroy(qbTimer* timer) {
  delete *timer;
  *timer = nullptr;
  return QB_OK;
}

void qb_timer_start(qbTimer timer) {
#ifdef __COMPILE_AS_WINDOWS__
  timer->start_ = qb_timer_now();
#elif defined (__COMPILE_AS_LINUX__)
  clock_gettime(CLOCK_TYPE, &start_);
#endif
}

void qb_timer_stop(qbTimer timer) {
#ifdef __COMPILE_AS_WINDOWS__
  timer->end_ = qb_timer_now();
#elif defined (__COMPILE_AS_LINUX__)
  clock_gettime(CLOCK_TYPE, &end_);
#endif
}

int64_t qb_timer_add(qbTimer timer) {
  int64_t elapsed = 0;
#ifdef __COMPILE_AS_WINDOWS__
  int64_t now = qb_timer_now();
  elapsed = now - timer->start_;
#elif defined (__COMPILE_AS_LINUX__)
  timespec tmp = diff(start_, end_);
  elapsed = (NS_TO_SEC * tmp.tv_sec) + tmp.tv_nsec;
#endif

  if (timer->window_size_) {
    timer->window_[timer->iterator_++] = elapsed;
    timer->iterator_ %= timer->window_size_;
  }
  return elapsed;
}

void qb_timer_reset(qbTimer timer) {
#ifdef __COMPILE_AS_WINDOWS__
  timer->start_ = timer->end_ = 0;
#elif defined (__COMPILE_AS_LINUX__)
  start_.tv_sec = end_.tv_sec = 0;
  start_.tv_nsec = end_.tv_nsec = 0;
#endif
  timer->iterator_ = 0;
  for (uint8_t i = 0; i < timer->window_size_; ++i) {
    timer->window_[i] = 0;
  }
}

int64_t qb_timer_elapsed(qbTimer timer) {
#ifdef __COMPILE_AS_WINDOWS__
  return convert_to_time(timer->end_ - timer->start_);
#elif defined (__COMPILE_AS_LINUX__)
  timespec tmp = diff(start_, end_);
  return (NS_TO_SEC * tmp.tv_sec) + tmp.tv_nsec;
#endif
}

int64_t qb_timer_average(qbTimer timer) {
  if (timer->window_size_ == 0) {
    return qb_timer_elapsed(timer);
  }

  uint64_t accum = 0;
  for (auto& n : timer->window_) {
    accum += n;
  }
  return convert_to_time(accum / timer->window_size_);
}
