#ifndef VFS__H
#define VFS__H

#include <cubez/common.h>

typedef struct qbVfsAttr_ {
  char** argv;
  wchar_t** wargv;
} qbVfsAttr_, *qbVfsAttr;

void vfs_initialize(qbVfsAttr attr);

#endif  // VFS__H