#ifndef COLLISION_UTILS__H
#define COLLISION_UTILS__H

#include <cubez/mesh.h>
#include "mesh_builder.h"

namespace collision_utils {

// Mesh-mesh collision.
bool CollidesWith(const glm::vec3& a_origin, const qbCollider_* a,
                  const glm::vec3& b_origin, const qbCollider_* b);

// Mesh-ray collision.
bool CollidesWith(const glm::vec3& a_origin, const qbCollider_* a,
                  const qbRay_* ray);

// Mesh-point collision.
bool CollidesWith(const glm::vec3& a_origin, const qbCollider_* a,
                  const glm::vec3& b_origin);

bool CheckAabb(const glm::vec3& a_origin, const glm::vec3& b_origin,
          const qbCollider_* a, const qbCollider_* b);

bool CheckGjk(const glm::vec3& a_origin, const glm::vec3& b_origin,
         const qbCollider_* a, const qbCollider_* b);

}
#endif  // COLLISION_UTILS__H