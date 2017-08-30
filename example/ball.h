#ifndef BALL__H
#define BALL__H

#include <string>

#include <glm/glm.hpp>
#include <SDL2/SDL.h>

#include "inc/cubez.h"
#include "inc/schema.h"

namespace ball {

const char kCollection[] = "ball_objects";

struct Settings {
  std::string texture;
  std::string vs;
  std::string fs;
};

struct Object {
  qbId physics_id;
  qbId render_id;
};

typedef cubez::Schema<uint32_t, Object> Objects;

void initialize(const Settings& settings);

qbId create(glm::vec3 pos, glm::vec3 vel);

}  // namespace ball
#endif
