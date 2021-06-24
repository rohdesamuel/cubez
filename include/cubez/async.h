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

#define QB_TASK_MAX_NUM_INPUTS 8

typedef struct qbChannel_* qbChannel;
typedef struct qbQueue_* qbQueue;
typedef struct qbTask_* qbTask;

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

QB_API qbChannel  qb_task_input(qbTask task, uint8_t input);

QB_API qbVar      qb_task_join(qbTask task);

#endif  // CUBEZ_ASYNC__H