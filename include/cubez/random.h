#ifndef CUBEZ_RANDOM__H
#define CUBEZ_RANDOM__H

#include "common.h"

typedef struct qbSplitmix_ {
  uint64_t s;
} qbSplitmix_, *qbSplitmix;

typedef struct qbXorshift_ {
  uint64_t s;
} qbXorshift_, *qbXorshift;

typedef struct qbXorshift128p_ {
  uint64_t s[2];
} qbXorshift128p_, *qbXorshift128p;

typedef struct qbXoshiro256ss_ {
  uint64_t s[4];
} qbXoshiro256ss_, *qbXoshiro256ss;

QB_API void qb_seed(uint64_t s);
QB_API uint64_t qb_rand();

QB_API uint64_t qb_splitmix(qbSplitmix state);
QB_API uint64_t qb_xorshift(qbXorshift state);
QB_API uint64_t qb_xorshift128p(qbXorshift128p state);
QB_API uint64_t qb_xoshiro256ss(qbXoshiro256ss state);

#endif  // CUBEZ_RANDOM__H