#ifndef HASH__H
#define HASH__H

#include <cubez/common.h>

QB_API uint64_t qb_hash_bytes(const uint8_t* start, size_t length);
QB_API uint64_t qb_hash_incr(uint64_t h, uint64_t val);
QB_API uint64_t qb_hash_finish(uint64_t h);
QB_API uint64_t qb_hash_combine(uint64_t a, uint64_t b);

#endif  // HASH__H