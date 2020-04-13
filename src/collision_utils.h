/**
* Author: Samuel Rohde (rohde.samuel@cubez.io)
*
* Copyright {2020} {Samuel Rohde}
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

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