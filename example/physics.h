#ifndef PHYSICS__H
#define PHYSICS__H

#include "constants.h"
#include "generic.h"

#include "inc/cubez.h"

#include <glm/glm.hpp>

namespace physics {


struct Material {
  float mass;
  
  // Constant of restitution.
  float r;
};

struct Transform {
  glm::vec3 p;
  glm::vec3 v;
};

struct Impulse {
  qbId key;
  glm::vec3 p;
};

struct Settings {
  float gravity = 0.0f;
  float friction = 0.0f;
};

void initialize(const Settings& settings);
qbId create(glm::vec3 pos, glm::vec3 vel);
void send_impulse(qbEntity entity, glm::vec3 p);

qbComponent component();
qbComponent collidables();
qbComponent movables();

}


#endif  // PHYSICS__H
