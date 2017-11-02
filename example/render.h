#ifndef RENDER__H
#define RENDER__H

#include <string>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <set>
#include <unordered_map>
#include <SDL2/SDL.h>

#include "inc/cubez.h"

namespace render {

typedef struct qbRenderable_* qbRenderable;

struct Material {
  glm::vec4 color;
  unsigned int shader_id;
  unsigned int texture_id;
  glm::vec2 texture_offset;
  glm::vec2 texture_scale;
};

struct Mesh {
  unsigned int vbo;
  unsigned int vao;
  unsigned int count;
};

struct RenderEvent {
  uint64_t frame;
  uint64_t ftimestamp_us;
};

struct Settings {
  std::string title;
  int width;
  int height;
  float fov;
  float znear;
  float zfar;
};

void initialize(const Settings& settings);
void shutdown();
int window_height();
int window_width();

qbRenderable create(const Material& material, const Mesh& mesh);

qbComponent component();

uint32_t load_texture(const std::string& file);

void add_transform(qbId renderable_id, qbId transform_id);

void render(qbId material, qbId mesg);

void present(RenderEvent* event);

}  // namespace render

#endif
