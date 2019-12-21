#ifndef CUBEZ_LOG__H
#define CUBEZ_LOG__H

#include <cubez/cubez.h>

typedef enum {
  QB_DEBUG = -1,
  QB_INFO = 0,
  QB_WARN = 1,
  QB_ERR = 2,
} qbLogLevel;

QB_API extern const char* QB_STD_OUT;
QB_API extern const char* QB_STD_ERR;

QB_API void qb_log(qbLogLevel level, const char* format, ...);
QB_API void qb_out(qbLogLevel level, const char* dst, const char* format, ...);

#endif  // CUBEZ_LOG__H
