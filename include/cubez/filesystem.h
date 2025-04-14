#ifndef CUBEZ_FILESYSTEM__H
#define CUBEZ_FILESYSTEM__H

#include <cubez/common.h>
#include <cubez/buffer.h>

typedef void* qbFile;

typedef struct qbBufferOrResult_ {
  qbBool has_val;
  union {
    qbResult res;
    qbBuffer_ val;
  };
} qbBufferOrResult_;

QB_API qbResult qb_fopen(qbFile* file, const utf8_t* path, const char* mode);
QB_API qbBool qb_fexists(const utf8_t* path);
QB_API qbResult qb_fclose(qbFile file);
QB_API qbResult qb_fread(qbFile file, qbBuffer buf, size_t size, size_t count, size_t* nread);
QB_API qbBufferOrResult_ qb_fload(const utf8_t* path);
QB_API void qb_ffree(qbBuffer buf);

typedef enum qbOrigin {
  QB_ORIGIN_SET,
  QB_ORIGIN_CUR,
  QB_ORIGIN_END,
};
QB_API qbResult qb_fseek(qbFile file, int32_t pos, qbOrigin seek_mode);
QB_API qbResult qb_ftell(qbFile file, int32_t* pos);

#endif  // CUBEZ_FILESYSTEM__H