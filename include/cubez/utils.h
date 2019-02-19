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
API int64_t qb_timer_query();

API qbResult qb_timer_create(qbTimer* timer, uint8_t window_size);
API qbResult qb_timer_destroy(qbTimer* timer);

API void qb_timer_start(qbTimer timer);

API void qb_timer_stop(qbTimer timer);

API void qb_timer_add(qbTimer timer);

API void qb_timer_reset(qbTimer timer);

API int64_t qb_timer_elapsed(qbTimer timer);

API int64_t qb_timer_average(qbTimer timer);

#endif  // CUBEZ_UTILS__H