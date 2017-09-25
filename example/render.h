#ifndef RENDER__H
#define RENDER__H

#include <string>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <set>
#include <unordered_map>
#include <SDL2/SDL.h>

#include "inc/cubez.h"
#include "inc/schema.h"
#include "inc/pool.h"

namespace render {

const char kRenderables[] = "renderables";
const char kMaterials[] = "materials";
const char kMeshes[] = "meshes";

struct Renderable {
  qbId material_id;
  qbId mesh_id;
  std::set<qbId> transform_ids;
};

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

typedef cubez::Schema<uint32_t, Renderable> Renderables;
typedef cubez::Schema<uint32_t, Material> Materials;
typedef cubez::Schema<uint32_t, Mesh> Meshes;

void initialize();
qbId create(const Material& material,
            const Mesh& mesh);

uint32_t load_texture(const std::string& file);

void add_transform(qbId renderable_id, qbId transform_id);

void render(qbId material, qbId mesg);

void present(RenderEvent* event);

}  // namespace render

#endif
