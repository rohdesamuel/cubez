#ifndef PLAYER__H
#define PLAYER__H

#include "inc/cubez.h"
#include "inc/schema.h"

#include <glm/glm.hpp>

namespace player {

const char kCollection[] = "player";

struct Object {
  glm::vec3 color;
};

struct Settings {
  glm::vec3 start_pos;
  glm::vec3 color;
};

void initialize(const Settings& settings);

void move_left(float speed);
void move_right(float speed);
void move_up(float speed);
void move_down(float speed);

}  // namespace player

#endif  // PLAYER__H