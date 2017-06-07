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
  uint32_t texture_id;
  cubez::Id physics_id;
  cubez::Id render_id;
  cubez::Id shader_progam;
};

typedef cubez::Schema<uint32_t, Object> Objects;

void initialize(const Settings& settings);

cubez::Id create(glm::vec3 pos, glm::vec3 vel,  const std::string& tex,
                 const std::string& vs, const std::string& fs);

}  // namespace ball
#endif
