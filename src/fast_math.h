#ifndef FAST_MATH__H
#define FAST_MATH__H

#include <cubez/common.h>

namespace fast_math
{

int32_t log_2(uint64_t v);

uint32_t count_bits(uint32_t v);
uint32_t count_bits(uint64_t v);

}  // namespace fast_math

#endif /*FAST_MATH__H*/