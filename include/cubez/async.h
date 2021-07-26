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

#ifndef CUBEZ_ASYNC__H
#define CUBEZ_ASYNC__H

#include <cubez/cubez.h>

typedef struct qbChannel_* qbChannel;
typedef struct qbQueue_* qbQueue;
typedef qbHandle qbTask;
typedef struct qbTaskBundle_* qbTaskBundle;


// Creates a multi-publisher single-subscriber thread-safe channel.
// Has strong sequential consistency guarantee, e.g. two writes from two
// different threads will be read in FIFO order.
QB_API void qb_channel_create(qbChannel* channel);
QB_API void qb_channel_destroy(qbChannel* channel);

// Writes the given var to the channel. Blocks until is able to write.
QB_API void qb_channel_write(qbChannel channel, qbVar v);

// Reads a var from the channel. Blocks until data is available.
QB_API void qb_channel_read(qbChannel channel, qbVar* v);

// Reads a var from the channel. Blocks until data is available.
QB_API void qb_channel_read(qbChannel channel, qbVar* v);

// Iterates through channels in a random order until data is available.
QB_API qbVar qb_channel_select(qbChannel* channels, uint8_t len);

// Creates a multi-publisher, multi-subscriper thread-safe queue.
// There is no guarantee of sequential consistency, e.g. two writes from the
// same thread may not be read in FIFO order.
QB_API void qb_queue_create(qbQueue* queue);
QB_API void qb_queue_destroy(qbQueue* queue);
QB_API void qb_queue_write(qbQueue queue, qbVar v);
QB_API qbBool qb_queue_tryread(qbQueue queue, qbVar* v);

QB_API qbTask   qb_task_async(qbVar(*entry)(qbTask, qbVar), qbVar var);
QB_API qbVar    qb_task_join(qbTask task);
QB_API qbBool   qb_task_isactive(qbTask task);

typedef struct qbTaskBundleAttr_ {
  uint64_t reserved;
} qbTaskBundleAttr_, *qbTaskBundleAttr;

typedef struct qbTaskBundleBeginInfo_ {
  uint64_t reserved;
} qbTaskBundleBeginInfo_, *qbTaskBundleBeginInfo;

typedef struct qbTaskBundleAddInfo_ {
  uint64_t reserved;
} qbTaskBundleAddTaskInfo_, *qbTaskBundleAddTaskInfo;

typedef struct qbTaskBundleSubmitInfo_ {
  uint64_t reserved;
} qbTaskBundleSubmitInfo_, *qbTaskBundleSubmitInfo;

QB_API qbTaskBundle qb_taskbundle_create(qbTaskBundleAttr attr);
QB_API void qb_taskbundle_begin(qbTaskBundle bundle, qbTaskBundleBeginInfo info);
QB_API void qb_taskbundle_end(qbTaskBundle bundle);
QB_API void qb_taskbundle_clear(qbTaskBundle bundle);

QB_API void qb_taskbundle_addtask(qbTaskBundle bundle, qbVar(*entry)(qbTask, qbVar), qbTaskBundleAddTaskInfo info);
QB_API void qb_taskbundle_addquery(qbTaskBundle bundle, qbQuery query, qbTaskBundleAddTaskInfo info);
QB_API void qb_taskbundle_addsystem(qbTaskBundle bundle, qbSystem system, qbTaskBundleAddTaskInfo info);
QB_API void qb_taskbundle_addbundle(qbTaskBundle bundle, qbTaskBundle tasks,
                                    qbTaskBundleAddTaskInfo add_info,
                                    qbTaskBundleSubmitInfo submit_info);
QB_API void qb_taskbundle_addsleep(qbTaskBundle bundle, uint64_t duration_ms, qbTaskBundleAddTaskInfo info);

QB_API qbTask qb_taskbundle_submit(qbTaskBundle bundle, qbVar arg, qbTaskBundleSubmitInfo info);
QB_API qbTask qb_taskbundle_dispatch(qbTaskBundle bundle, qbVar arg, qbTaskBundleSubmitInfo info);
QB_API qbVar  qb_taskbundle_run(qbTaskBundle bundle, qbVar arg);

#endif  // CUBEZ_ASYNC__H