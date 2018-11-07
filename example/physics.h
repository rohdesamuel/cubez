#ifndef PHYSICS__H
#define PHYSICS__H

#include "inc/cubez.h"
#include "mesh_builder.h"

#include <glm/glm.hpp>
#include <vector>

namespace physics {

struct Transform {
  glm::vec3 p;
  glm::vec3 v;
  bool is_fixed;
};

struct Impulse {
  qbId key;
  glm::vec3 p;
};

struct qbCollision {
  qbEntity a;
  qbEntity b;
};

struct qbCollisionMesh {
  std::vector<glm::vec3> convex_hull;

  glm::vec3 max;
  glm::vec3 min;
};

struct qbCollidable {
  Mesh* collision_mesh;

  float r;
};

struct Settings {
  float gravity = 0.0f;
  float friction = 0.0f;
};

void initialize(const Settings& settings);
qbId create(glm::vec3 pos, glm::vec3 vel);
void send_impulse(qbEntity entity, glm::vec3 p);

qbComponent component();
qbComponent collidable();
qbComponent collision();

qbResult on_collision(qbSystem system);

}


#endif  // PHYSICS__H
