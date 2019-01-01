#ifndef COLLISION_UTILS__H
#define COLLISION_UTILS__H

#include "mesh.h"
#include "mesh_builder.h"

namespace collision_utils {

// Mesh-mesh collision.
bool CollidesWith(const glm::vec3& a_origin, const Mesh& a,
                  const glm::vec3& b_origin, const Mesh& b);

// Mesh-ray collision.
bool CollidesWith(const glm::vec3& a_origin, const Mesh& a,
                  const Ray& ray);

// Mesh-point collision.
bool CollidesWith(const glm::vec3& a_origin, const Mesh& a,
                  const glm::vec3& b_origin);

bool CheckAabb(const glm::vec3& a_origin, const glm::vec3& b_origin,
          const Mesh& a, const Mesh& b);

bool CheckGjk(const glm::vec3& a_origin, const glm::vec3& b_origin,
         const Mesh& a, const Mesh& b);

}
#endif  // COLLISION_UTILS__H