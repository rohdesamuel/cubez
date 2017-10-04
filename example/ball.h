#ifndef BALL__H
#define BALL__H

#include <string>

#include <glm/glm.hpp>
#include <SDL2/SDL.h>

#include "inc/cubez.h"

namespace ball {

const char kCollection[] = "ball_objects";

struct Settings {
  std::string texture;
  std::string vs;
  std::string fs;
};

void initialize(const Settings& settings);

void create(glm::vec3 pos, glm::vec3 vel);

}  // namespace ball
#endif
