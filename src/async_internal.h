#ifndef ASYNC_INTERNAL__H
#define ASYNC_INTERNAL__H

#include <cubez/cubez.h>

void async_initialize(qbSchedulerAttr_* attr);
void async_stop();

#endif  // ASYNC_INTERNAL__H