#ifndef COLLISION_UTILS__H
#define COLLISION_UTILS__H

#include <cubez/mesh.h>
#include "mesh_builder.h"

namespace collision_utils {

// Mesh-mesh collision.
bool CollidesWith(const vec3s& a_origin, const qbCollider_* a,
                  const vec3s& b_origin, const qbCollider_* b);

// Mesh-ray collision.
bool CollidesWith(const vec3s& a_origin, const qbCollider_* a,
                  const qbRay_* ray);

// Mesh-point collision.
bool CollidesWith(const vec3s& a_origin, const qbCollider_* a,
                  const vec3s& b_origin);

bool CheckAabb(const vec3s& a_origin, const vec3s& b_origin,
               const qbCollider_* a, const qbCollider_* b);

bool CheckGjk(const vec3s& a_origin, const vec3s& b_origin,
              const qbCollider_* a, const qbCollider_* b);

}
#endif  // COLLISION_UTILS__H