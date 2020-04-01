#include <cubez/mesh.h>
#include <cubez/render.h>
#include "mesh_builder.h"
#include "shader.h"

#include <glm/glm.hpp>
#include <GL/glew.h>

#include <iostream>
#include <fstream>
#include <map>
#include <cstring>
#include <string>
#include <tuple>
#include <array>
#include <vector>
#include <cubez/cubez.h>

#include "render_defs.h"

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#ifdef _DEBUG
#define CHECK_GL()  {if (GLenum err = glGetError()) FATAL(gluErrorString(err)) }
#else
#define CHECK_GL()
#endif

qbResult process_line(MeshBuilder* builder, const std::string& token, 
                      const std::string& line) {
  if (token == "v") {
    glm::vec3 v = {0, 0, 0};
    if (SSCANF(line.c_str(), "%f %f %f", &v.x, &v.y, &v.z) == 3) {
      builder->AddVertex(std::move(v));
    }
  } else if (token == "vt") {
    glm::vec2 vt = {0, 0};
    if (SSCANF(line.c_str(), "%f %f", &vt.x, &vt.y) == 2) {
      builder->AddTexture(std::move(vt));
    }
  } else if (token == "vn") {
    glm::vec3 vn = {0, 0, 0};
    if (SSCANF(line.c_str(), "%f %f %f", &vn.x, &vn.y, &vn.z) == 3) {
      builder->AddNormal(std::move(vn));
    }
  } else if (token == "f") {
    MeshBuilder::Face face;
    if (SSCANF(line.c_str(),
                 "%d/%d/%d %d/%d/%d %d/%d/%d",
                 &face.v[0], &face.vt[0], &face.vn[0],
                 &face.v[1], &face.vt[1], &face.vn[1],
                 &face.v[2], &face.vt[2], &face.vn[2]) == 9) {
      // Convert from 1-indexing to 0-indexing.
      for (int i = 0; i < 3; ++i) {
        --face.v[i];
        --face.vn[i];
        --face.vt[i];
      }
      builder->AddFace(std::move(face));
    }
  } else if (token != "#") {
    // Bad format
    return QB_UNKNOWN;
  }
  return QB_OK;
}

QB_API qbResult qb_model_load(qbRenderable* renderable, qbModel* model,
                       const char* name, const char* file) {
  return QB_OK;
}

qbResult process_line(MeshBuilder* builder, const std::string& line) {
  if (line.empty() || line == "\n") {
    return QB_OK;
  }
  size_t token_end = line.find(' ');
  if (token_end == std::string::npos) {
    return QB_UNKNOWN;
  }
  std::string token = line.substr(0, token_end);

  return process_line(builder, token, line.substr(token_end, line.size() - token_end));
}

qbResult qb_mesh_load(qbMesh* mesh, const char*, const char* filename) {
  std::ifstream file;
  file.open(filename, std::ios::in);
  MeshBuilder builder;
  if (file.is_open()) {
    std::string line;
    while(getline(file, line)) {
      if (process_line(&builder, line) != qbResult::QB_OK) {
        file.close();
        return qbResult::QB_UNKNOWN;
      }
    }
  } else {
    file.close();
    return qbResult::QB_UNKNOWN;
  }
  file.close();
  assert(false && "unimplemented");
  //*mesh = builder.Mesh(qbRenderMode::QB_TRIANGLES);
  return qbResult::QB_OK;
}

void qb_model_upload(qbModel model, struct qbRenderable_** renderable) {
  assert(false && "unimplemented");
}

void qb_model_destroy(qbModel* model) {
  qbModel m = *model;
  delete[] m->meshes;
  delete[] m->colliders;
  delete *model;
  *model = nullptr;
}

qbResult qb_mesh_destroy(qbMesh* mesh) {
  
  return QB_OK;
}

bool qb_model_collides(glm::vec3* a_origin, glm::vec3* b_origin, qbModel a, qbModel b) {
  assert(false && "unimplemented");
  return false;
}

qbResult qb_mesh_tobuffer(qbMesh mesh, qbMeshBuffer* buffer) {
  qbRenderer renderer = qb_renderer();
  *buffer = renderer->meshbuffer_create(renderer, mesh);
  return QB_OK;
}

qbResult qb_material_create(qbMaterial* material, qbMaterialAttr attr, const char* material_namem) {
  qbMaterial m = *material = new qbMaterial_;
  m->albedo_map = attr->albedo_map;
  m->normal_map = attr->normal_map;
  m->metallic_map = attr->metallic_map;
  m->roughness_map = attr->roughness_map;
  m->ao_map = attr->ao_map;
  m->emission_map = attr->emission_map;

  m->albedo = attr->albedo;
  m->metallic = attr->metallic;
  m->roughness = attr->roughness;
  m->emission = attr->emission;

  m->image_count = attr->image_count;
  m->images = new qbImage[m->image_count];
  m->image_units = new uint32_t[m->image_count];
  memcpy(m->images, attr->images, m->image_count * sizeof(qbImage));
  memcpy(m->image_units, attr->image_units, m->image_count * sizeof(uint32_t));

  m->uniform_count = attr->uniform_count;
  m->uniforms = new qbGpuBuffer[m->uniform_count];
  m->uniform_bindings = new uint32_t[m->uniform_count];
  memcpy(m->uniforms, attr->uniforms, m->uniform_count * sizeof(qbGpuBuffer));
  memcpy(m->uniform_bindings, attr->uniform_bindings, m->uniform_count * sizeof(uint32_t));

  return QB_OK;
}

qbResult qb_material_destroy(qbMaterial* material) {
  qbMaterial m = *material;
  delete[] m->images;
  delete[] m->image_units;
  delete[] m->uniforms;
  delete[] m->uniform_bindings;
  delete m;
  *material = nullptr;

  return QB_OK;
}

glm::vec3 qb_collider_farthest(const qbCollider_* collider, glm::vec3 dir) {
  float max_dot = 0.0f;
  int ret = 0;
  for (; ret < collider->count; ++ret) {
    float dot = glm::dot(collider->vertices[ret], dir);
    if (dot > max_dot) {
      max_dot = dot;
    }
  }

  return collider->vertices[ret];
}

struct qbRenderable_* qb_draw_cube(int size_x, int size_y, int size_z) {
  MeshBuilder builder = MeshBuilder::Box(size_x, size_y, size_z);
  qbRenderable_* ret;
  qb_renderable_create(&ret, builder.Model(qbRenderFaceType_::QB_TRIANGLES));
  return ret;
}

struct qbRenderable_* qb_draw_rect(int w, int h) {
  MeshBuilder builder = MeshBuilder::Rect(w, h);
  qbRenderable_* ret;
  qb_renderable_create(&ret, builder.Model(qbRenderFaceType_::QB_TRIANGLES));
  return ret;
}

struct qbRenderable_* qb_draw_sphere(float radius, int slices, int zslices) {
  MeshBuilder builder = MeshBuilder::Sphere(radius, slices, zslices);
  qbRenderable_* ret;
  qb_renderable_create(&ret, builder.Model(qbRenderFaceType_::QB_TRIANGLES));
  return ret;
}