#ifndef BALL__H
#define BALL__H

#include <string>

#include <glm/glm.hpp>

#include "inc/cubez.h"
#include "mesh.h"

namespace ball {

const char kCollection[] = "ball_objects";

struct Settings {
  qbMesh mesh;
  qbMaterial material;
};

void initialize(const Settings& settings);

void create(glm::vec3 pos, glm::vec3 vel, bool collidable = true);

qbComponent Component();

}  // namespace ball
#endif
