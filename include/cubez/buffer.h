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

// A buffer is a portable (between different hosts) method of packing data. The
// memory in the buffer is owned by the user. The bytes are written in network
// order.
typedef struct qbBuffer_ {
  size_t capacity;
  uint8_t* bytes;
} qbBuffer_, *qbBuffer;

// Writes the given bytes to the buffer and returns the number of bytes
// written. Writes `size` number of bytes until the buffer size reaches the
// capacity. Increments `pos` with the number of bytes written. If the bytes
// length is dynamic and needs a length-prefix, then use qb_buffer_writestr. 
QB_API size_t qb_buffer_write(qbBuffer buf, ptrdiff_t* pos, size_t size, const void* bytes);

// Reads the bytes from the buffer at the given position and returns the number
// of bytes read. Increments `pos` with the number of bytes read. If the bytes
// length is dynamic and has a length-prefix, then use qb_buffer_readstr.
QB_API size_t qb_buffer_read(const qbBuffer_* buf, ptrdiff_t* pos, size_t size, void* bytes);

// Writes [len: 8 bytes | s: `len` bytes] to the buffer at the given position.
// Returns the number of bytes written. Increments `pos` with the number of
// bytes written.
QB_API size_t qb_buffer_writestr(qbBuffer buf, ptrdiff_t* pos, size_t len, const char* s);

// Writes [n: 8 bytes] to the buffer at the given position. Returns the number
// of bytes written. Increments `pos` with the number of bytes written.
QB_API size_t qb_buffer_writell(qbBuffer buf, ptrdiff_t* pos, int64_t n);

// Writes [n: 4 bytes] to the buffer at the given position. Returns the number
// of bytes written. Increments `pos` with the number of bytes written.
QB_API size_t qb_buffer_writel(qbBuffer buf, ptrdiff_t* pos, int32_t n);

// Writes [n: 2 bytes] to the buffer at the given position. Returns the number
// of bytes written. Increments `pos` with the number of bytes written.
QB_API size_t qb_buffer_writes(qbBuffer buf, ptrdiff_t* pos, int16_t n);

// Writes [c: 1 byte] to the buffer at the given position. Returns the number
// of bytes written. Increments `pos` with the number of bytes written.
QB_API size_t qb_buffer_writec(qbBuffer buf, ptrdiff_t* pos, uint8_t c);

// Writes [n: 8 bytes] to the buffer at the given position. Returns the number
// of bytes written. Increments `pos` with the number of bytes written.
QB_API size_t qb_buffer_writed(qbBuffer buf, ptrdiff_t* pos, double n);

// Writes [n: 4 bytes] to the buffer at the given position. Returns the number
// of bytes written. Increments `pos` with the number of bytes written.
QB_API size_t qb_buffer_writef(qbBuffer buf, ptrdiff_t* pos, float n);


// Reads the length-prefixed string from the buffer starting at the given
// position. Returns the number of bytes read. Increments `pos` with the number
// of bytes read.
QB_API size_t qb_buffer_readstr(const qbBuffer_* buf, ptrdiff_t* pos, size_t* len, char** s);

// Reads 8 bytes from the buffer starting at the given position. Returns the
// number of bytes read. Increments `pos` with the number of bytes read.
QB_API size_t qb_buffer_readll(const qbBuffer_* buf, ptrdiff_t* pos, int64_t* n);

// Reads 4 bytes from the buffer starting at the given position. Returns the
// number of bytes read. Increments `pos` with the number of bytes read.
QB_API size_t qb_buffer_readl(const qbBuffer_* buf, ptrdiff_t* pos, int32_t* n);

// Reads 2 bytes from the buffer starting at the given position. Returns the
// number of bytes read. Increments `pos` with the number of bytes read.
QB_API size_t qb_buffer_reads(const qbBuffer_* buf, ptrdiff_t* pos, int16_t* n);

// Reads 1 byte from the buffer starting at the given position. Returns the
// number of bytes read. Increments `pos` with the number of bytes read.
QB_API size_t qb_buffer_readc(const qbBuffer_* buf, ptrdiff_t* pos, uint8_t* c);

// Reads 8 bytes from the buffer starting at the given position as a double.
// Returns the number of bytes read. Increments `pos` with the number of bytes
// read.
QB_API size_t qb_buffer_readd(const qbBuffer_* buf, ptrdiff_t* pos, double* n);

// Reads 4 bytes from the buffer starting at the given position as a float.
// Returns the number of bytes read. Increments `pos` with the number of bytes
// read.
QB_API size_t qb_buffer_readf(const qbBuffer_* buf, ptrdiff_t* pos, float* n);

#endif  // CUBEZ_BUFFER__H