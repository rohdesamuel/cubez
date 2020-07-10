/**
* Author: Samuel Rohde (rohde.samuel@cubez.io)
*
* Copyright 2020 Samuel Rohde
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

#include "collision_utils.h"
#include "gjk.h"
#include "simplex.h"

#include <algorithm>
#include <iostream>
#include <cglm/struct/vec3.h>
#include <assert.h>

#include <reactphysics3d/reactphysics3d.h>

#if 0
namespace {

vec3s Support(const vec3s& a_origin, const vec3s& b_origin,
              const qbCollider_* shape1, const qbCollider_* shape2, vec3s d) {
  // d is a vector direction (doesn't have to be normalized)
  // get points on the edge of the shapes in opposite directions
  vec3s p1 = {};
  qb_collider_support(shape1, d);
  p1 = glms_vec3_add(a_origin, p1);

  vec3s p2 = {};
  vec3s neg_d = glms_vec3_negate(d);
  qb_collider_support(shape2, neg_d);
  p2 = glms_vec3_add(b_origin, p2);

  // perform the Minkowski Difference
  vec3s p3 = glms_vec3_sub(p1, p2);
  
  // p3 is now a point in Minkowski space on the edge of the Minkowski Difference
  return p3;
}

bool ContainsOrigin(Simplex* s, vec3s* dir) {
  // get the last point added to the simplex
  vec3s a = (*s)[0];

  // compute AO (same thing as -A)
  vec3s ao = glms_vec3_negate(a);
  if (s->Size() == 4) {
    // then its the triangle case
    vec3s b = (*s)[1];
    vec3s c = (*s)[2];
    vec3s d = (*s)[3];
    // compute the edges
    vec3s ab = glms_vec3_sub(b, a);
    vec3s ac = glms_vec3_sub(c, a);
    vec3s ad = glms_vec3_sub(d, a);
    // compute the face normal
    vec3s cbPerp = glms_vec3_cross(ac, ab);
    vec3s dcPerp = glms_vec3_cross(ad, ac);
    vec3s bdPerp = glms_vec3_cross(ab, ad);
    // is the origin in R4
    if (glms_vec3_dot(cbPerp, ao) > 0) {
      // remove point d
      s->Remove(std::move(d));
      // set the new direction to abPerp
      *dir = cbPerp;
    } else if (glms_vec3_dot(dcPerp, ao) > 0) {
      // remove point b
      s->Remove(std::move(b));
      // set the new direction to abPerp
      *dir = dcPerp;
    } else if (glms_vec3_dot(bdPerp, ao) > 0) {
      // is the origin in R3
      // remove point c
      s->Remove(std::move(c));
      // set the new direction to acPerp
      *dir = bdPerp;
    } else {
      // otherwise we know its in R5 so we can return true
      return true;
    }
  } else if (s->Size() == 3) {
    // then its the line segment case
    // compute AB
    vec3s b = (*s)[1];
    vec3s c = (*s)[2];
    vec3s ab = glms_vec3_sub(b, a);
    vec3s ac = glms_vec3_sub(c, a);
    // get the perp to AB in the direction of the origin
    vec3s abPerp = glms_vec3_cross(ab, ac);
    // set the direction to abPerp
    if (glms_vec3_dot(abPerp, ao) > 0) {
      // set the new direction to abPerp
      *dir = abPerp;
    } else if (glms_vec3_dot(abPerp, ao) < 0) {
      // set the new direction to abPerp
      *dir = glms_vec3_negate(abPerp);
    }
  } else if (s->Size() == 2) {
    vec3s a = (*s)[0];
    vec3s b = (*s)[1];
    vec3s ab = glms_vec3_sub(b, a);
    *dir = glms_vec3_cross(glms_vec3_cross(ab, glms_vec3_negate(b)), ab);
  }
  return false;
}

}

namespace collision_utils {

bool CollidesWith(const vec3s& a_origin, const qbCollider_* a,
                  const vec3s& b_origin, const qbCollider_* b) {
  if (CheckAabb(a_origin, b_origin, a, b)) {
    return CheckGjk(a_origin, b_origin, a, b);
  }
  return false;
}

// https://stackoverflow.com/questions/6554313/algorithm-for-determining-whether-a-point-is-inside-a-3d-mesh
// https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm
bool CollidesWith(const vec3s& a_origin, const qbCollider_* a,
                  const qbRay_* r) {
  // https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-box-intersection
  float tmin = (a_origin.x + a->min.x - r->orig.x) / r->dir.x;
  float tmax = (a_origin.x + a->max.x - r->orig.x) / r->dir.x;

  if (tmin > tmax) std::swap(tmin, tmax);

  float tymin = (a_origin.y + a->min.y - r->orig.y) / r->dir.y;
  float tymax = (a_origin.y + a->max.y - r->orig.y) / r->dir.y;

  if (tymin > tymax) std::swap(tymin, tymax);

  if ((tmin > tymax) || (tymin > tmax))
    return false;

  if (tymin > tmin)
    tmin = tymin;

  if (tymax < tmax)
    tmax = tymax;

  float tzmin = (a_origin.z + a->min.z - r->orig.z) / r->dir.z;
  float tzmax = (a_origin.z + a->max.z - r->orig.z) / r->dir.z;

  if (tzmin > tzmax) std::swap(tzmin, tzmax);

  if ((tmin > tzmax) || (tzmin > tmax))
    return false;

  if (tzmin > tmin)
    tmin = tzmin;

  if (tzmax < tmax)
    tmax = tzmax;
  
  return true;
}

bool CheckAabb(const vec3s& a_origin, const vec3s& b_origin,
               const qbCollider_* a, const qbCollider_* b) {
  return
    a_origin.x + a->min.x <= b_origin.x + b->max.x && a_origin.x + a->max.x >= b_origin.x + b->min.x &&
    a_origin.y + a->min.y <= b_origin.y + b->max.y && a_origin.y + a->max.y >= b_origin.y + b->min.y &&
    a_origin.z + a->min.y <= b_origin.z + b->max.z && a_origin.z + a->max.z >= b_origin.z + b->min.z;
}

bool CheckGjk(const vec3s& a_origin, const vec3s& b_origin,
              const qbCollider_* a, const qbCollider_* b) {
  /*
  http://allenchou.net/2013/12/game-physics-collision-detection-gjk/
  http://allenchou.net/2013/12/game-physics-contact-generation-epa/
  https://doc.cgal.org/latest/Convex_hull_3/index.html#Chapter_3D_Convex_Hulls
  https://www.haroldserrano.com/blog/visualizing-the-gjk-collision-algorithm
  http://www.dyn4j.org/2010/04/gjk-gilbert-johnson-keerthi/#gjk-collision
  https://www.medien.ifi.lmu.de/lehre/ss10/ps/Ausarbeitung_Beispiel.pdf
  https://www.gamedev.net/forums/topic/604497-farthest-point-in-a-direction/
  */

  // TODO: improve this guess.
  // choose a search direction
  vec3s d = glms_vec3_sub(b_origin, a_origin);
  if (glms_vec3_norm(d) < 0.0001) {
    d = vec3s{ 1.0, 0.0, 0.0 };
  }
  // get the first Minkowski Difference point
  Simplex s;
  s.Add(Support(a_origin, b_origin, a, b, d));
  // negate d for the next point
  d = glms_vec3_negate(d);
  // start looping
  int max_iterations = 50;
  while (max_iterations > 0) {
    // add a new point to the simplex because we haven't terminated yet
    s.Add(Support(a_origin, b_origin, a, b, d));
    // make sure that the last point we added actually passed the origin
    if (glms_vec3_dot(s.Last(), d) <= 0) {
      // if the point added last was not past the origin in the direction of d
      // then the Minkowski Sum cannot possibly contain the origin since
      // the last point added is on the edge of the Minkowski Difference
      return false;
    } else {
      // otherwise we need to determine if the origin is in
      // the current simplex
      if (ContainsOrigin(&s, &d)) {
        // if it does then we know there is a collision
        return true;
      }
    }
    --max_iterations;
  }
  return false;
}

}
#endif