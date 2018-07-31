#include "collision_utils.h"
#include "gjk.h"

#include <iostream>

class Simplex {
public:
  Simplex() {}
  size_t Size() const;
  void Add(glm::vec3 p);
  void Remove(glm::vec3 p);

  glm::vec3 Last() {
    return points_.front();
  };

  glm::vec3& operator[](size_t i);
  const glm::vec3& operator[](size_t i) const;

private:
  std::vector<glm::vec3> points_;
};

size_t Simplex::Size() const {
  return points_.size();
}

void Simplex::Add(glm::vec3 p) {
  points_.push_back(std::move(p));
  std::swap(points_.front(), points_.back());
}

void Simplex::Remove(glm::vec3 p) {
  auto it = std::find_if(points_.begin(), points_.end(),
                         [&p](const glm::vec3& point) { return p == point; });
  std::swap(*it, points_.back());
  points_.pop_back();
}

glm::vec3& Simplex::operator[](size_t i) {
  return points_[i];
}

const glm::vec3& Simplex::operator[](size_t i) const {
  return points_[i];
}

namespace {

glm::vec3 Support(const glm::vec3& a_origin, const glm::vec3& b_origin,
                  const Mesh& shape1, const Mesh& shape2, glm::vec3 d) {
  // d is a vector direction (doesn't have to be normalized)
  // get points on the edge of the shapes in opposite directions
  glm::vec3 p1 = a_origin + shape1.FarthestPoint(d);
  glm::vec3 p2 = b_origin + shape2.FarthestPoint(-d);
  // perform the Minkowski Difference
  glm::vec3 p3 = p1 - p2;
  // p3 is now a point in Minkowski space on the edge of the Minkowski Difference
  return p3;
}

bool ContainsOrigin(Simplex* s, glm::vec3* dir) {
  // get the last point added to the simplex
  glm::vec3 a = (*s)[0];

  // compute AO (same thing as -A)
  glm::vec3 ao = -a;
  if (s->Size() == 4) {
    // then its the triangle case
    glm::vec3 b = (*s)[1];
    glm::vec3 c = (*s)[2];
    glm::vec3 d = (*s)[3];
    // compute the edges
    glm::vec3 ab = b - a;
    glm::vec3 ac = c - a;
    glm::vec3 ad = d - a;
    // compute the face normal
    glm::vec3 cbPerp = glm::cross(ac, ab);
    glm::vec3 dcPerp = glm::cross(ad, ac);
    glm::vec3 bdPerp = glm::cross(ab, ad);
    // is the origin in R4
    if (glm::dot(cbPerp, ao) > 0) {
      // remove point d
      s->Remove(d);
      // set the new direction to abPerp
      *dir = cbPerp;
    } else if (glm::dot(dcPerp, ao) > 0) {
      // remove point b
      s->Remove(b);
      // set the new direction to abPerp
      *dir = dcPerp;
    } else if (glm::dot(bdPerp, ao) > 0) {
      // is the origin in R3
      // remove point c
      s->Remove(c);
      // set the new direction to acPerp
      *dir = bdPerp;
    } else {
      // otherwise we know its in R5 so we can return true
      return true;
    }
  } else if (s->Size() == 3) {
    // then its the line segment case
    // compute AB
    glm::vec3 b = (*s)[1];
    glm::vec3 c = (*s)[2];
    glm::vec3 ab = b - a;
    glm::vec3 ac = c - a;
    // get the perp to AB in the direction of the origin
    glm::vec3 abPerp = glm::cross(ab, ac);
    // set the direction to abPerp
    if (glm::dot(abPerp, ao) > 0) {
      // set the new direction to abPerp
      *dir = abPerp;
    } else if (glm::dot(abPerp, ao) < 0) {
      // set the new direction to abPerp
      *dir = -abPerp;
    }
  } else if (s->Size() == 2) {
    glm::vec3 a = (*s)[0];
    glm::vec3 b = (*s)[1];
    glm::vec3 ab = b - a;
    *dir = glm::cross(glm::cross(ab, -b), ab);
  }
  return false;
}

}

namespace collision_utils {


bool CollidesWith(const glm::vec3& a_origin, const Mesh& a,
                  const glm::vec3& b_origin, const Mesh& b) {
  if (CheckAabb(a_origin, b_origin, a, b)) {
    return CheckGjk(a_origin, b_origin, a, b);
  }
  return false;
}

bool CollidesWith(const glm::vec3& a_origin, const Mesh& a,
                  const Ray& r) {
  // https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-box-intersection
  float tmin = (a_origin.x + a.Min().x - r.orig.x) / r.dir.x;
  float tmax = (a_origin.x + a.Max().x - r.orig.x) / r.dir.x;

  if (tmin > tmax) std::swap(tmin, tmax);

  float tymin = (a_origin.y + a.Min().y - r.orig.y) / r.dir.y;
  float tymax = (a_origin.y + a.Max().y - r.orig.y) / r.dir.y;

  if (tymin > tymax) std::swap(tymin, tymax);

  if ((tmin > tymax) || (tymin > tmax))
    return false;

  if (tymin > tmin)
    tmin = tymin;

  if (tymax < tmax)
    tmax = tymax;

  float tzmin = (a_origin.z + a.Min().z - r.orig.z) / r.dir.z;
  float tzmax = (a_origin.z + a.Max().z - r.orig.z) / r.dir.z;

  if (tzmin > tzmax) std::swap(tzmin, tzmax);

  if ((tmin > tzmax) || (tzmin > tmax))
    return false;

  if (tzmin > tmin)
    tmin = tzmin;

  if (tzmax < tmax)
    tmax = tzmax;
  
  return true;
}

bool CheckAabb(const glm::vec3& a_origin, const glm::vec3& b_origin,
          const Mesh& a, const Mesh& b) {
  return
    a_origin.x + a.Min().x <= b_origin.x + b.Max().x && a_origin.x + a.Max().x >= b_origin.x + b.Min().x &&
    a_origin.y + a.Min().y <= b_origin.y + b.Max().y && a_origin.y + a.Max().y >= b_origin.y + b.Min().y &&
    a_origin.z + a.Min().z <= b_origin.z + b.Max().z && a_origin.z + a.Max().z >= b_origin.z + b.Min().z;
}

bool CheckGjk(const glm::vec3& a_origin, const glm::vec3& b_origin,
         const Mesh& a, const Mesh& b) {
  /*
  http://allenchou.net/2013/12/game-physics-collision-detection-gjk/
  http://allenchou.net/2013/12/game-physics-contact-generation-epa/
  https://doc.cgal.org/latest/Convex_hull_3/index.html#Chapter_3D_Convex_Hulls
  https://www.haroldserrano.com/blog/visualizing-the-gjk-collision-algorithm
  http://www.dyn4j.org/2010/04/gjk-gilbert-johnson-keerthi/#gjk-collision
  https://www.medien.ifi.lmu.de/lehre/ss10/ps/Ausarbeitung_Beispiel.pdf
  https://www.gamedev.net/forums/topic/604497-farthest-point-in-a-direction/
  */
  Gjk gjk;
  return gjk.IsColliding(a, b);
#if 0
  // TODO: improve this guess.
  // choose a search direction
  glm::vec3 d = b_origin - a_origin;
  if (glm::length(d) < 0.0001) {
    d = glm::vec3{ 1.0, 0.0, 0.0 };
  }
  // get the first Minkowski Difference point
  Simplex s;
  s.Add(Support(a_origin, b_origin, a, b, d));
  // negate d for the next point
  d = -d;
  // start looping
  int max_iterations = 50;
  while (max_iterations > 0) {
    // add a new point to the simplex because we haven't terminated yet
    s.Add(Support(a_origin, b_origin, a, b, d));
    // make sure that the last point we added actually passed the origin
    if (glm::dot(s.Last(), d) <= 0) {
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
#endif
}

}