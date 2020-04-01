#include "gjk.h"

namespace {

glm::vec3 TripleProduct(const glm::vec3& a, const glm::vec3& b) {
  return glm::cross(glm::cross(a, b), a);
}

}

namespace collision_utils {

Gjk::Gjk() {
  a = b = c = d = glm::vec3();
}

Gjk::~Gjk() {
}

bool Gjk::IsColliding(const Mesh& mesh_a, const Mesh& mesh_b) {
  glm::vec3 dir = glm::vec3(1, 1, 1);

  c = support(mesh_a, mesh_b, dir);

  dir = -c;//negative direction

  b = support(mesh_a, mesh_b, dir);

  if (glm::dot(b, dir) < 0) {
    return false;
  }
  dir = TripleProduct(c - b, -b);

  nrPointsSimplex = 2; //begin with 2 points in simplex

  int steps = 0;//avoid infinite loop
  while (steps < 50000000) {
    a = support(mesh_a, mesh_b, dir);
    if (glm::dot(a, dir) < 0) {
      return false;
    } else {
      if (ContainsOrigin(dir)) {
        return true;
      }
    }
    steps++;

  }

  return false;
}
bool Gjk::ContainsOrigin(glm::vec3& dir) {
  if (nrPointsSimplex == 2) {
    return triangle(dir);
  } else if (nrPointsSimplex == 3) {
    return tetrahedron(dir);
  }

  return false;
}

bool Gjk::line(glm::vec3& dir) {
  glm::vec3 ab = b - a;
  glm::vec3 ao = -a;//glm::vec3::zero() - a

                  //can t be behind b;

                  //new direction towards a0
  dir = TripleProduct(ab, ao);

  c = b;
  b = a;
  nrPointsSimplex = 2;

  return false;
}

bool Gjk::triangle(glm::vec3& dir) {
  glm::vec3 ao = glm::vec3(-a.x, -a.y, -a.z);
  glm::vec3 ab = b - a;
  glm::vec3 ac = c - a;
  glm::vec3 abc = glm::cross(ab, ac);

  //point is can't be behind/in the direction of B,C or BC


  glm::vec3 ab_abc = glm::cross(ab, abc);
  // is the origin away from ab edge? in the same plane
  //if a0 is in that direction than
  if (glm::dot(ab_abc, ao) > 0) {
    //change points
    c = b;
    b = a;

    //dir is not ab_abc because it's not point towards the origin
    dir = TripleProduct(ab, ao);

    //direction change; can't build tetrahedron
    return false;
  }


  glm::vec3 abc_ac = glm::cross(abc, ac);

  // is the origin away from ac edge? or it is in abc?
  //if a0 is in that direction than
  if (glm::dot(abc_ac, ao) > 0) {
    //keep c the same
    b = a;

    //dir is not abc_ac because it's not point towards the origin
    dir = TripleProduct(ac, ao);

    //direction change; can't build tetrahedron
    return false;
  }

  //now can build tetrahedron; check if it's above or below
  if (glm::dot(abc, ao) > 0) {
    //base of tetrahedron
    d = c;
    c = b;
    b = a;

    //new direction
    dir = abc;
  } else {
    //upside down tetrahedron
    d = b;
    b = a;
    dir = -abc;
  }

  nrPointsSimplex = 3;

  return false;
}

bool Gjk::tetrahedron(glm::vec3& dir) {
  glm::vec3 ao = -a;//0-a
  glm::vec3 ab = b - a;
  glm::vec3 ac = c - a;

  //build abc triangle
  glm::vec3 abc = glm::cross(ab, ac);

  //CASE 1
  if (glm::dot(abc, ao) > 0) {
    //in front of triangle ABC
    //we don't have to change the ao,ab,ac,abc meanings
    checkTetrahedron(ao, ab, ac, abc, dir);
  }


  //CASE 2:

  glm::vec3 ad = d - a;

  //build acd triangle
  glm::vec3 acd = glm::cross(ac, ad);

  //same direaction with ao
  if (glm::dot(acd, ao) > 0) {

    //in front of triangle ACD
    b = c;
    c = d;
    ab = ac;
    ac = ad;
    abc = acd;

    checkTetrahedron(ao, ab, ac, abc, dir);
  }

  //build adb triangle
  glm::vec3 adb = glm::cross(ad, ab);

  //case 3:

  //same direaction with ao
  if (glm::dot(adb, ao) > 0) {

    //in front of triangle ADB

    c = b;
    b = d;

    ac = ab;
    ab = ad;

    abc = adb;
    checkTetrahedron(ao, ab, ac, abc, dir);
  }


  //origin in tetrahedron
  return true;

}

bool Gjk::checkTetrahedron(const glm::vec3& ao,
                           const glm::vec3& ab,
                           const glm::vec3& ac,
                           const glm::vec3& abc,
                           glm::vec3& dir) {

  //almost the same like triangle checks
  glm::vec3 ab_abc = glm::cross(ab, abc);

  if (glm::dot(ab_abc, ao) > 0) {
    c = b;
    b = a;

    //dir is not ab_abc because it's not point towards the origin;
    //ABxA0xAB direction we are looking for
    dir = TripleProduct(ab, ao);

    //build new triangle
    // d will be lost
    nrPointsSimplex = 2;

    return false;
  }

  glm::vec3 acp = glm::cross(abc, ac);

  if (glm::dot(acp, ao) > 0) {
    b = a;

    //dir is not abc_ac because it's not point towards the origin;
    //ACxA0xAC direction we are looking for
    dir = TripleProduct(ac, ao);

    //build new triangle
    // d will be lost
    nrPointsSimplex = 2;

    return false;
  }

  //build new tetrahedron with new base
  d = c;
  c = b;
  b = a;

  dir = abc;

  nrPointsSimplex = 3;

  return false;
}
glm::vec3 Gjk::support(const Mesh& a, const Mesh& b, const glm::vec3& dir) const {
  glm::vec3 p1 = a.FarthestPoint(dir);
  glm::vec3 p2 = b.FarthestPoint(-dir);

  //p2 -p1
  glm::vec3 p3 = p1 - p2;

  return  p3;
}

}