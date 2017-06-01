#ifndef BALL__H
#define BALL__H

#include <string>

#include <glm/glm.hpp>
#include <SDL2/SDL.h>

#include "inc/cubez.h"
#include "inc/schema.h"

namespace ball {

const char kCollection[] = "ball_objects";

struct Object {
  uint32_t texture_id;
  cubez::Id physics_id;
  cubez::Id render_id;
};

typedef cubez::Schema<uint32_t, Object> Objects;

void initialize();
cubez::Id create(glm::vec3 pos, glm::vec3 vel, const std::string& texture);

}  // namespace ball
#endif
