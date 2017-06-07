#ifndef RENDER__H
#define RENDER__H

#include <string>

#include <glm/glm.hpp>
#include <SDL2/SDL.h>

#include "inc/cubez.h"
#include "inc/schema.h"

namespace render {

const char kCollection[] = "render_objects";

struct Object {
  unsigned int shader_program;
  unsigned int vbo;
  unsigned int vao;
};

struct RenderEvent {
  uint64_t frame;
  uint64_t ftimestamp_us;
};

typedef cubez::Schema<uint32_t, Object> Objects;

void initialize();
cubez::Id create(Object* render_info);

void present(RenderEvent* event);

}  // namespace render

typedef render::Object RenderInfo;

#endif

