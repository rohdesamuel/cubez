#ifndef BALL__H
#define BALL__H

#include <string>

#include <glm/glm.hpp>

#include <cubez/cubez.h>
#include <cubez/mesh.h>

namespace ball {

const char kCollection[] = "ball_objects";

struct Settings {
  qbMesh mesh;
  qbMesh* collision_mesh;
  qbMaterial material;
  qbMaterial material_exploded;
};

void initialize(const Settings& settings);

qbEntity create(glm::vec3 pos, glm::vec3 vel, bool explodable = true,
                bool collidable = false);

qbComponent Component();

}  // namespace ball
#endif
