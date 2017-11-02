#ifndef PLAYER__H
#define PLAYER__H

#include "inc/cubez.h"

#include <glm/glm.hpp>
#include <string>

namespace player {

struct Settings {
  glm::vec3 start_pos;
  std::string texture;
  std::string vs;
  std::string fs;
};

void initialize(const Settings& settings);

void move_left(float speed);
void move_right(float speed);
void move_up(float speed);
void move_down(float speed);

glm::mat4 perspective();

}  // namespace player

#endif  // PLAYER__H

