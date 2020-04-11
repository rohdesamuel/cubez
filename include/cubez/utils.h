/**
* Author: Samuel Rohde (rohde.samuel@gmail.com)
*
* This file is subject to the terms and conditions defined in
* file 'LICENSE.txt', which is part of this source code package.
*/

#ifndef CUBEZ_UTILS__H
#define CUBEZ_UTILS__H

#include "common.h"

typedef struct qbTimer_* qbTimer;

// Returns a strictly increasing monotonic timestamp with units of nanoseconds.
QB_API int64_t qb_timer_query();

QB_API qbResult qb_timer_create(qbTimer* timer, uint8_t window_size);
QB_API qbResult qb_timer_destroy(qbTimer* timer);

QB_API void qb_timer_start(qbTimer timer);

QB_API void qb_timer_stop(qbTimer timer);

QB_API void qb_timer_add(qbTimer timer);

QB_API void qb_timer_reset(qbTimer timer);

QB_API int64_t qb_timer_elapsed(qbTimer timer);

QB_API int64_t qb_timer_average(qbTimer timer);

#endif  // CUBEZ_UTILS__H