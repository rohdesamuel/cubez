#ifndef MESH__H
#define MESH__H

#include "inc/cubez.h"

typedef struct qbMesh_* qbMesh;

qbResult qb_mesh_load(qbMesh* mesh, const char* filename);
qbResult qb_mesh_destroy();

#endif   // MESH__H
