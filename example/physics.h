#ifndef PHYSICS__H
#define PHYSICS__H

#include "constants.h"

#include "inc/cubez.h"
#include "inc/schema.h"

#include <glm/glm.hpp>

namespace physics {

const char kCollection[] = "physics_objects";

struct Material {
  float mass;
  
  // Constant of restitution.
  float r;
};

struct Object {
  glm::vec3 p;
  glm::vec3 v;
};

struct Impulse {
  uint32_t key;
  glm::vec3 p;
};

struct Settings {
  float gravity = 0.0f;
  float friction = 0.0f;
};

typedef cubez::Schema<uint32_t, Object> Objects;

void initialize(const Settings& settings);
cubez::Id create(glm::vec3 pos, glm::vec3 vel);
void send_impulse(Objects::Key key, glm::vec3 p);

cubez::Collection* get_collection();

}


#endif  // PHYSICS__H
