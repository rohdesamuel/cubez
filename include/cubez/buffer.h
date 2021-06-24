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

#ifndef CUBEZ_BUFFER__H
#define CUBEZ_BUFFER__H

#include <cubez/common.h>

typedef struct qbBuffer_ {
  const size_t capacity;
  uint8_t* bytes;
} qbBuffer_, *qbBuffer;

QB_API size_t qb_buffer_write(qbBuffer buf, ptrdiff_t* pos, size_t size, const void* bytes);
QB_API size_t qb_buffer_read(const qbBuffer_* buf, ptrdiff_t* pos, size_t size, void* bytes);

QB_API size_t qb_buffer_writestr(qbBuffer buf, ptrdiff_t* pos, size_t len, const char* s);
QB_API size_t qb_buffer_writell(qbBuffer buf, ptrdiff_t* pos, int64_t n);
QB_API size_t qb_buffer_writel(qbBuffer buf, ptrdiff_t* pos, int32_t n);
QB_API size_t qb_buffer_writes(qbBuffer buf, ptrdiff_t* pos, int16_t n);
QB_API size_t qb_buffer_writed(qbBuffer buf, ptrdiff_t* pos, double n);
QB_API size_t qb_buffer_writef(qbBuffer buf, ptrdiff_t* pos, float n);

QB_API size_t qb_buffer_readstr(const qbBuffer_* buf, ptrdiff_t* pos, size_t* len, char** s);
QB_API size_t qb_buffer_readll(const qbBuffer_* buf, ptrdiff_t* pos, int64_t* n);
QB_API size_t qb_buffer_readl(const qbBuffer_* buf, ptrdiff_t* pos, int32_t* n);
QB_API size_t qb_buffer_reads(const qbBuffer_* buf, ptrdiff_t* pos, int16_t* n);
QB_API size_t qb_buffer_readd(const qbBuffer_* buf, ptrdiff_t* pos, double* n);
QB_API size_t qb_buffer_readf(const qbBuffer_* buf, ptrdiff_t* pos, float* n);

#endif  // CUBEZ_BUFFER__H