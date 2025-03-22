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

#ifndef CUBEZ_UTILS__H
#define CUBEZ_UTILS__H

#include "common.h"

typedef struct qbTimer_* qbTimer;

// Returns an increasing monotonic timestamp measured from the engine start
// without time spent paused with units of nanoseconds.

// qb_time() -> time of game start - time paused
QB_API int64_t qb_time();

// Returns a strictly increasing monotonic timestamp measured from the engine
// start with units of nanoseconds.
QB_API int64_t qb_systime();

QB_API qbResult qb_timer_create(qbTimer* timer, uint8_t window_size);
QB_API qbResult qb_timer_destroy(qbTimer* timer);

QB_API void qb_timer_start(qbTimer timer);

QB_API void qb_timer_stop(qbTimer timer);

QB_API int64_t qb_timer_add(qbTimer timer);

QB_API void qb_timer_reset(qbTimer timer);

QB_API int64_t qb_timer_elapsed(qbTimer timer);

QB_API int64_t qb_timer_average(qbTimer timer);

// Sleeps for the given duration in milliseconds.
QB_API void qb_sleep(uint32_t ms);

// Sleeps for the given duration in microseconds using high-resolution,
// OS-specific constructs.
QB_API void qb_highres_sleep(uint32_t us);


#endif  // CUBEZ_UTILS__H