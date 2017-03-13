#ifndef PARTICLES__H
#define PARTICLES__H

#include "inc/cubez.h"
#include "inc/schema.h"

#include <glm/glm.hpp>

const char kMainProgram[] = "main";
const char kParticleCollection[] = "particles";

struct Particle {
  glm::vec3 p;
  glm::vec3 v;
};

typedef cubez::Schema<uint32_t, Particle> Particles;

cubez::Collection* add_particle_collection(const char* collection, uint64_t particle_count);

void add_particle_pipeline(const char* collection);

#endif  // PARTICLES__H
