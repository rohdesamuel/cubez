#include "fast_math.h"

int32_t log_2(uint64_t v) {
#define LT(n) n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n
  static const char LogTable256[] =
  {
    -1, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
    LT(4), LT(5), LT(5), LT(6), LT(6), LT(6), LT(6),
    LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7), LT(7)
  };
  uint64_t t; // temporary
  if ((t = v) >> 56) {
    return 56 + LogTable256[(uint8_t)t];
  } else if ((t = v) >> 48) {
    return 48 + LogTable256[(uint8_t)t];
  } else if ((t = v) >> 40) {
    return 40 + LogTable256[(uint8_t)t];
  } else if ((t = v) >> 32) {
    return 32 + LogTable256[(uint8_t)t];
  } else if ((t = v) >> 24) {
    return 24 + LogTable256[(uint8_t)t];
  } else if ((t = v) >> 16) {
    return 16 + LogTable256[(uint8_t)t];
  } else if ((t = v) >> 8) {
    return 8 + LogTable256[(uint8_t)t];
  } else {
    return LogTable256[(uint8_t)v];
  }
}

uint32_t count_bits(uint32_t v) {
  static const unsigned char BitsSetTable256[256] =
  {
#   define B2(n) n,     n+1,     n+1,     n+2
#   define B4(n) B2(n), B2(n+1), B2(n+1), B2(n+2)
#   define B6(n) B4(n), B4(n+1), B4(n+1), B4(n+2)
    B6(0), B6(1), B6(1), B6(2)
  };
  return
    BitsSetTable256[v & 0xff] +
    BitsSetTable256[(v >> 8) & 0xff] +
    BitsSetTable256[(v >> 16) & 0xff] +
    BitsSetTable256[v >> 24];
}

uint32_t count_bits(uint64_t v) {
  static const unsigned char BitsSetTable256[256] =
  {
#   define B2(n) n,     n+1,     n+1,     n+2
#   define B4(n) B2(n), B2(n+1), B2(n+1), B2(n+2)
#   define B6(n) B4(n), B4(n+1), B4(n+1), B4(n+2)
    B6(0), B6(1), B6(1), B6(2)
  };
  return
    BitsSetTable256[v & 0xff] +
    BitsSetTable256[(v >> 8) & 0xff] +
    BitsSetTable256[(v >> 16) & 0xff] +
    BitsSetTable256[(v >> 24) & 0xff] +
    BitsSetTable256[(v >> 32) & 0xff] +
    BitsSetTable256[(v >> 40) & 0xff] +
    BitsSetTable256[(v >> 48) & 0xff] +
    BitsSetTable256[v >> 56];
}
