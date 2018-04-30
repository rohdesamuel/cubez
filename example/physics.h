#ifndef PHYSICS__H
#define PHYSICS__H

#include "constants.h"

#include "inc/cubez.h"

#include <glm/glm.hpp>

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

struct Collision {
  qbEntity a;
  qbEntity b;
};

struct Collidable {
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
qbComponent movable();

qbResult on_collision(qbSystem system);

}


#endif  // PHYSICS__H
