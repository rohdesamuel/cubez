#ifndef ASYNC_INTERNAL__H
#define ASYNC_INTERNAL__H

#include <cubez/cubez.h>
#include <cubez/async.h>
#include <functional>

void async_initialize(qbSchedulerAttr_* attr);
void async_stop();


void qb_taskbundle_addtask(qbTaskBundle bundle, std::function<qbVar(qbTask, qbVar)>&& entry, qbTaskBundleAddTaskInfo info);

#endif  // ASYNC_INTERNAL__H