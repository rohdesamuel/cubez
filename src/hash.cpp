#include <cubez/hash.h>

uint64_t qb_hash_bytes(const uint8_t* start, size_t length) {
  uint64_t h = 0;
  size_t i = 0;
  size_t num_words = length / sizeof(h);
  for (size_t i = 0; i < num_words; ++i) {
    uint64_t v =
      (uint64_t)(start[7]) << 56 | (uint64_t)(start[6]) << 48 |
      (uint64_t)(start[5]) << 40 | (uint64_t)(start[4]) << 32 |
      (uint64_t)(start[3]) << 24 | (uint64_t)(start[2]) << 16 |
      (uint64_t)(start[1]) << 8 | (uint64_t)(start[0]) << 0;

    h = qb_hash_incr(h, v);
    start += sizeof(h);
  }

  uint64_t v = 0;
  for (size_t i = 0; i < length % sizeof(h); ++i) {
    v |= (uint64_t)(start[i]) << (8 * i);
  }
  h = qb_hash_incr(h, v);

  return qb_hash_finish(h);
}

uint64_t qb_hash_incr(uint64_t h, uint64_t val) {
  h += val;
  h += h << 10;
  h ^= h >> 6;

  return h;
}

uint64_t qb_hash_finish(uint64_t h) {
  h += h << 3;
  h ^= h >> 11;
  h += h << 15;

  return h * 1181783497276652981ULL;
}

uint64_t qb_hash_combine(uint64_t a, uint64_t b) {
  return qb_hash_finish(qb_hash_incr(a, b));
}