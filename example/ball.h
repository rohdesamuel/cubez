#ifndef BALL__H
#define BALL__H

#include <string>

#include <glm/glm.hpp>

#include "inc/cubez.h"
#include "mesh.h"
#include "mesh_builder.h"

namespace ball {

const char kCollection[] = "ball_objects";

struct Settings {
  qbMesh mesh;
  Mesh* collision_mesh;
  qbMaterial material;
  qbMaterial material_exploded;
};

void initialize(const Settings& settings);

void create(glm::vec3 pos, glm::vec3 vel, bool explodable = true,
            bool collidable = false);

qbComponent Component();

}  // namespace ball
#endif
