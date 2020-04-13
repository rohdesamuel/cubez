/**
* Author: Samuel Rohde (rohde.samuel@cubez.io)
*
* Copyright {2020} {Samuel Rohde}
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include <cubez/mesh.h>
#include <cubez/render.h>
#include "mesh_builder.h"
#include "shader.h"

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
#include <assert.h>

#include "render_defs.h"
#include <cglm/struct.h>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#ifdef _DEBUG
#define CHECK_GL()  {if (GLenum err = glGetError()) FATAL(gluErrorString(err)) }
#else
#define CHECK_GL()
#endif

struct qbRenderable_* qb_model_load(const char* model_name, const char* filename) {
  MeshBuilder builder = MeshBuilder::FromFile(filename);
  qbRenderable_* ret;
  qb_renderable_create(&ret, builder.Model(qbRenderFaceType_::QB_TRIANGLES));
  return ret;
}

void qb_model_destroy(qbModel* model) {
  qbModel m = *model;
  delete[] m->meshes;
  delete[] m->colliders;
  delete *model;
  *model = nullptr;
}

bool qb_model_collides(vec3 a_origin, vec3 b_origin, qbModel a, qbModel b) {
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

vec3s qb_collider_farthest(const qbCollider_* collider, vec3s dir) {
  float max_dot = 0.0f;
  int ret = 0;
  for (; ret < collider->count; ++ret) {
    float dot = glms_vec3_dot(collider->vertices[ret], dir);
    if (dot > max_dot) {
      max_dot = dot;
    }
  }

  return collider->vertices[ret];
}

struct qbRenderable_* qb_draw_cube(int size_x, int size_y, int size_z) {
  MeshBuilder builder = MeshBuilder::Box((float)size_x, (float)size_y, (float)size_z);
  qbRenderable_* ret;
  qb_renderable_create(&ret, builder.Model(qbRenderFaceType_::QB_TRIANGLES));
  return ret;
}

struct qbRenderable_* qb_draw_rect(int w, int h) {
  MeshBuilder builder = MeshBuilder::Rect((float)w, (float)h);
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