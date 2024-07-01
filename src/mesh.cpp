/**
* Author: Samuel Rohde (rohde.samuel@cubez.io)
*
* Copyright 2020 Samuel Rohde
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
#include <cubez/log.h>
#include <cubez/renderer.h>
#include "mesh_builder.h"
#include "shader.h"
#include "assimp/Importer.hpp"

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
#include <algorithm>
#include <filesystem>

#include "render_defs.h"
#include <cglm/struct.h>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#ifdef _DEBUG
#define CHECK_GL()  {if (GLenum err = glGetError()) FATAL(gluErrorString(err)) }
#else
#define CHECK_GL()
#endif

// Make sure that the vectors are byte compatible.
static_assert(sizeof(vec3s) == sizeof(aiVector3f));

static inline vec3s pos_to_world(vec3s p, const qbTransform_* t) {
  return glms_vec3_add(p, t->position);
}

qbMesh qb_mesh_load(const char* mesh_name, const utf8_t* filename) {
  auto resources = qb_resources();
  std::filesystem::path path = std::filesystem::path(qb_dir()) / qb_resources()->resources;
  if (resources->meshes) {
    path = path /qb_resources()->meshes;
  }

  path = path / filename;

  assert(std::filesystem::exists(path) && "Mesh does not exist.");

  MeshBuilder builder = MeshBuilder::FromFile(path.string().c_str());
  qbMesh ret = builder.Mesh(QB_DRAW_MODE_TRIANGLES);

  Assimp::Importer importer;
  const aiScene* scene = importer.ReadFile(path.string(), 0);
  
  qbRenderer r = qb_renderer();
  if (r) {
    r->mesh_create(r, ret);
  }

  return ret;
}

namespace {

qbMesh aimesh_to_qbmesh(const aiMesh* mesh) {
  if (mesh->GetNumUVChannels() > 1) {
    qb_err("qbMesh only supports at most 1-channel UV coordinates. Received %d "
           "channels.", mesh->GetNumUVChannels());
    return nullptr;
  }

  qbMesh copy = new qbMesh_{};
  copy->mode = QB_DRAW_MODE_TRIANGLES;
  copy->vertex_count = mesh->mNumVertices;
  copy->vertices = new vec3s[copy->vertex_count];
  memcpy(copy->vertices, mesh->mVertices, sizeof(vec3s) * copy->vertex_count);

  if (mesh->mNormals) {
    copy->normals = new vec3s[copy->vertex_count];
    memcpy(copy->normals, mesh->mNormals, sizeof(vec3s) * copy->vertex_count);
  }

  if (mesh->mTextureCoords) {
    copy->uvs = new vec2s[copy->vertex_count];
    for (size_t i = 0; i < mesh->mNumVertices; ++i) {
      copy->uvs[i] = vec2s{ mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
    }
  }

  if (mesh->mColors && mesh->GetNumColorChannels() > 0) {
    // 4 because Assimp uses vec4 as the color type.
    copy->color_channels = 4 * mesh->GetNumColorChannels();
    copy->colors = new float[copy->vertex_count * copy->color_channels];

    for (size_t i = 0; i < mesh->GetNumColorChannels(); ++i) {
      if (mesh->mColors[i]) {
        for (int j = 0; j < mesh->mNumVertices; ++j) {
          copy->colors[j * copy->color_channels + i * 4 + 0] = mesh->mColors[i][j].r;
          copy->colors[j * copy->color_channels + i * 4 + 1] = mesh->mColors[i][j].g;
          copy->colors[j * copy->color_channels + i * 4 + 2] = mesh->mColors[i][j].b;
          copy->colors[j * copy->color_channels + i * 4 + 3] = mesh->mColors[i][j].a;
        }
      }
    }
  }

  if (mesh->mFaces) {
    // Assuming that the importer used the "aiProcess_Triangulate" flag, the
    // number of indices in each face is 3.
    copy->index_count = mesh->mNumFaces * 3;
    copy->indices = new uint32_t[copy->index_count];
    for (size_t i = 0; i < mesh->mNumFaces; i++) {
      aiFace face = mesh->mFaces[i];
      for (size_t j = 0; j < face.mNumIndices; j++) {
        copy->indices[i * 3 + j] = face.mIndices[j];
      }
    }
  }

  return copy;
}

qbMaterial aimaterial_to_qbmaterial(const char* material_name, const aiMaterial* material) {
  qbMaterialAttr_ attr = {};

  aiColor3D col = {};
  ai_real real = 0.f;

  if (material->Get(AI_MATKEY_COLOR_DIFFUSE, col) == aiReturn_SUCCESS) {
    attr.color = { col.r, col.g, col.b };
  }

  if (material->Get(AI_MATKEY_COLOR_AMBIENT, col) == aiReturn_SUCCESS) {
    attr.ambient = { col.r, col.g, col.b };
  }

  if (material->Get(AI_MATKEY_COLOR_SPECULAR, col) == aiReturn_SUCCESS) {
    attr.specular = { col.r, col.g, col.b };
  }

  if (material->Get(AI_MATKEY_COLOR_EMISSIVE, col) == aiReturn_SUCCESS) {
    attr.emissive = { col.r, col.g, col.b };
  }

  if (material->Get(AI_MATKEY_METALLIC_FACTOR, real) == aiReturn_SUCCESS) {
    attr.metallic = real;
  }

  if (material->Get(AI_MATKEY_ROUGHNESS_FACTOR, real) == aiReturn_SUCCESS) {
    attr.roughness = real;
  }

  // TODO: add shaders as materials.
  // AI_MATKEY_SHADER_VERTEX
  // AI_MATKEY_SHADER_FRAGMENT

  aiString color_map_name;
  qbImageAttr_ image_attr = { .type = QB_IMAGE_TYPE_2D };
  if (material->Get(AI_MATKEY_TEXTURE_DIFFUSE(0), color_map_name) == aiReturn_SUCCESS ||
    material->Get(AI_MATKEY_BASE_COLOR, color_map_name) == aiReturn_SUCCESS) {
    qb_image_load(&attr.color_map, &image_attr, (utf8_t*)color_map_name.C_Str());
  }

  aiString ambient_map_name;
  if (material->Get(AI_MATKEY_TEXTURE_AMBIENT(0), ambient_map_name) == aiReturn_SUCCESS) {
    qb_image_load(&attr.ambient_map, &image_attr, (utf8_t*)ambient_map_name.C_Str());
  }

  aiString specular_map_name;
  if (material->Get(AI_MATKEY_TEXTURE_SPECULAR(0), specular_map_name) == aiReturn_SUCCESS) {
    qb_image_load(&attr.specular_map, &image_attr, (utf8_t*)specular_map_name.C_Str());
  }

  aiString emissive_map_name;
  if (material->Get(AI_MATKEY_TEXTURE_EMISSIVE(0), emissive_map_name) == aiReturn_SUCCESS) {
    qb_image_load(&attr.emissive_map, &image_attr, (utf8_t*)emissive_map_name.C_Str());
  }

  aiString normal_map_name;
  if (material->Get(AI_MATKEY_TEXTURE_NORMALS(0), normal_map_name) == aiReturn_SUCCESS) {
    qb_image_load(&attr.normal_map, &image_attr, (utf8_t*)normal_map_name.C_Str());
  }

  aiString metallic_map_name;
  if (material->Get(AI_MATKEY_USE_METALLIC_MAP(0), metallic_map_name) == aiReturn_SUCCESS) {
    qb_image_load(&attr.metallic_map, &image_attr, (utf8_t*)metallic_map_name.C_Str());
  }

  aiString roughness_map_name;
  if (material->Get(AI_MATKEY_USE_ROUGHNESS_MAP(0), roughness_map_name) == aiReturn_SUCCESS) {
    qb_image_load(&attr.roughness_map, &image_attr, (utf8_t*)roughness_map_name.C_Str());
  }

  aiString shininess_map_name;
  if (material->Get(AI_MATKEY_SHININESS(0), shininess_map_name) == aiReturn_SUCCESS ||
    material->Get(AI_MATKEY_TEXTURE_SHININESS(0), shininess_map_name) == aiReturn_SUCCESS) {
    qb_image_load(&attr.shininess_map, &image_attr, (utf8_t*)shininess_map_name.C_Str());
  }
  
  aiString ao_map_name;
  if (material->Get(AI_MATKEY_USE_AO_MAP(0), ao_map_name) == aiReturn_SUCCESS) {
    qb_image_load(&attr.ao_map, &image_attr, (utf8_t*)ao_map_name.C_Str());
  }

  qbMaterial ret;
  qb_material_create(&ret, &attr, material_name);

  return ret;
}

}

qbModel qb_model_load(const char* model_name, const utf8_t* filename) {
  auto resources = qb_resources();
  std::filesystem::path path(qb_dir());
  if (resources->meshes) {
    path = path / std::filesystem::path(qb_resources()->resources) / qb_resources()->meshes;
  } else {
    path = path / std::filesystem::path(qb_resources()->resources);
  }

  path = path / filename;

  qbModelAttr_ attr = {};
  qbModel ret = nullptr;
  Assimp::Importer importer{};
  const aiScene* scene = importer.ReadFile(path.string().c_str(), aiProcess_Triangulate | aiProcess_FlipUVs);
  if (!scene || !scene->HasMeshes()) {
    goto cleanup;
  }

  attr.mesh_count = scene->mNumMeshes;
  attr.meshes = (qbMesh*)alloca(sizeof(qbMesh) * attr.mesh_count);
  attr.material_binding_count = attr.mesh_count;
  attr.material_bindings = (uint32_t*)alloca(sizeof(uint32_t) * attr.material_binding_count);
  memset(attr.meshes, 0, sizeof(qbMesh) * attr.mesh_count);
  memset(attr.material_bindings, 0, sizeof(uint32_t) * attr.material_binding_count);
  for (size_t i = 0; i < scene->mNumMeshes; ++i) {
    aiMesh* mesh = scene->mMeshes[i];
    attr.meshes[i] = aimesh_to_qbmesh(mesh);
    attr.material_bindings[i] = mesh->mMaterialIndex;
    if (!attr.meshes[i]) {
      goto cleanup;
    }
  }

  attr.collider_count = scene->mNumMeshes;
  attr.colliders = (qbCollider*)alloca(sizeof(qbCollider) * attr.collider_count);
  memset(attr.colliders, 0, sizeof(qbCollider) * attr.collider_count);
  for (size_t i = 0; i < scene->mNumMeshes; ++i) {
    qbCollider collider = nullptr;
    qb_collider_frommesh(&collider, attr.meshes[i]);
    attr.colliders[i] = collider;
    if (!attr.colliders[i]) {
      goto cleanup;
    }
  }

  attr.material_count = scene->mNumMaterials;
  attr.materials = (qbMaterial*)alloca(sizeof(qbMaterial) * attr.material_count);
  memset(attr.materials, 0, sizeof(qbMaterial) * attr.material_count);
  for (size_t i = 0; i < attr.material_count; ++i) {
    qbMaterial material = nullptr;
    aiMaterial* ai_material = scene->mMaterials[i];
    material = aimaterial_to_qbmaterial(i == 0 ? "default_material" : ai_material->GetName().C_Str(), ai_material);
    attr.materials[i] = material;
    if (!attr.materials[i]) {
      goto cleanup;
    }
  }

  qb_model_create(&ret, &attr);

  return ret;

cleanup:
  qb_err("Could not import mesh \"%s\"", path.string().c_str());

  for (size_t i = 0; i < attr.mesh_count; ++i) {
    if (attr.meshes[i]) {
      qb_mesh_destroy(&attr.meshes[i]);
    }
  }

  for (size_t i = 0; i < attr.collider_count; ++i) {
    if (attr.colliders[i]) {
      qb_collider_destroy(&attr.colliders[i]);
    }
  }

  for (size_t i = 0; i < attr.material_count; ++i) {
    if (attr.materials[i]) {
      qb_material_destroy(&attr.materials[i]);
    }
  }

  return nullptr;
}

void qb_model_create(qbModel* model_ref, qbModelAttr attr) {
  qbModel model = *model_ref = new qbModel_{};
  model->name = STRDUP(attr->name);

  model->collider_count = attr->collider_count;
  model->colliders = new qbCollider[model->collider_count];
  memcpy(model->colliders, attr->colliders, (model->collider_count) * sizeof(qbCollider));
  
  model->mesh_count = attr->mesh_count;
  model->meshes = new qbMesh[model->mesh_count];
  memcpy(model->meshes, attr->meshes, (model->mesh_count) * sizeof(qbMesh));

  model->material_count = attr->material_count;
  model->materials = new qbMaterial[model->material_count];
  memcpy(model->materials, attr->materials, (model->material_count) * sizeof(qbMaterial));

  model->material_binding_count = attr->material_binding_count;
  model->material_bindings = new uint32_t[model->material_binding_count];
  memcpy(model->material_bindings, attr->material_bindings, (model->material_binding_count) * sizeof(uint32_t));
  (*model_ref)->mode = attr->meshes[0]->mode;
}

void qb_model_destroy(qbModel* model) {
  qbModel m = *model;
  if (m->meshes) {
    for (uint32_t i = 0; i < (*model)->mesh_count; ++i) {
      qb_mesh_destroy(&m->meshes[i]);
    }
    delete[] m->meshes;
  }

  if (m->colliders) {
    for (uint32_t i = 0; i < (*model)->collider_count; ++i) {
      qb_collider_destroy(&m->colliders[i]);
    }
    delete[] m->colliders;
  }

  if (m->materials) {
    for (uint32_t i = 0; i < (*model)->material_count; ++i) {
      qb_material_destroy(&m->materials[i]);
    }
    delete[] m->materials;
  }
  delete *model;
  *model = nullptr;
}

qbResult qb_collider_destroy(qbCollider* collider) {
  free((*collider)->vertices);
  delete *(collider);
  return QB_OK;
}

bool qb_model_collides(vec3 a_origin, vec3 b_origin, qbModel a, qbModel b) {
  assert(false && "unimplemented");
  return false;
}

qbResult qb_mesh_destroy(qbMesh* mesh) {
  auto r = qb_renderer();
  if (r) {
    r->mesh_destroy(r, *mesh);
  }

  delete[](*mesh)->vertices;
  delete[](*mesh)->indices;
  delete[](*mesh)->normals;
  delete[](*mesh)->uvs;
  delete[](*mesh)->colors;
  delete *mesh;
  *mesh = nullptr;
  return QB_OK;
}

qbResult qb_material_create(qbMaterial* material, qbMaterialAttr attr, const char* material_name) {
  qbMaterial m = *material = new qbMaterial_;
  m->name = STRDUP(material_name);
  m->ext = attr->ext;
  
  m->color_map = attr->color_map;
  m->ambient_map = attr->ambient_map;
  m->specular_map = attr->specular_map;
  m->emissive_map = attr->emissive_map;

  m->normal_map = attr->normal_map;
  m->metallic_map = attr->metallic_map;
  m->roughness_map = attr->roughness_map;
  m->shininess_map = attr->shininess_map;
  m->ao_map = attr->ao_map;

  m->color = attr->color;
  m->ambient = attr->ambient;
  m->specular = attr->specular;
  m->emissive = attr->emissive;

  m->metallic = attr->metallic;
  m->roughness = attr->roughness;
  m->shininess = attr->shininess;

  return QB_OK;
}

qbResult qb_material_destroy(qbMaterial* material) {
  qbMaterial m = *material;
  qb_renderext_destroy(&(*material)->ext);

  delete m;
  *material = nullptr;

  return QB_OK;
}

vec3s qb_collider_support(const qbCollider_* collider, const qbTransform_* transform, vec3s dir) {
  // Rotate dir to match the orientation of the collider.
  mat4s inv_rot = glms_mat4_inv(transform->orientation);
  dir = glms_mat4_mulv3(inv_rot, dir, 0.f);

  vec3s support = {};
  if (collider->vertices) {
    float max_dot = std::numeric_limits<float>::lowest();
    int found = 0;
    for (int i = 0; i < collider->vertex_count; ++i) {
      float dot = glms_vec3_dot(collider->vertices[i], dir);
      if (dot > max_dot) {
        max_dot = dot;
        found = i;
      }
    }

    support = collider->vertices[found];
  } else {
    support = collider->support(collider, dir);    
  }
  return glms_mat4_mulv3(transform->orientation, glms_vec3_mul(transform->scale, support), 0.f);
}

struct qbPortal_ {
  vec3s v0, v1, v2, v3;
};

struct MinkowskiDifference {
  const qbCollider_* a;
  const qbCollider_* b;
  const qbTransform_* a_transform;
  const qbTransform_* b_transform;
};

float plane_plane_distance(const vec3s* p1, const vec3s* p2, const vec3s* n) {
  // The distance between two parallel planes is the same as subtracting their
  // distance from a known point on each plane to the origin.
  // http://wwwf.imperial.ac.uk/metric/metric_public/vectors/vector_coordinate_geometry/distance_between_planes.html
  float norm = glms_vec3_norm(*n);
  float d_1 = glms_vec3_dot(*p1, *n) / norm;
  float d_2 = glms_vec3_dot(*p2, *n) / norm;
  return std::abs(d_1 - d_2);
}

qbBool qb_collider_checkaabb(const qbCollider_* a, const qbCollider_* b,
                             const qbTransform_* a_t, const qbTransform_* b_t) {
  return
    a_t->position.x + a->min.x <= b_t->position.x + b->max.x &&
    a_t->position.x + a->max.x >= b_t->position.x + b->min.x &&
    a_t->position.y + a->min.y <= b_t->position.y + b->max.y &&
    a_t->position.y + a->max.y >= b_t->position.y + b->min.y &&
    a_t->position.z + a->min.y <= b_t->position.z + b->max.z &&
    a_t->position.z + a->max.z >= b_t->position.z + b->min.z;
}

qbBool qb_ray_checktri(const qbRay_* ray, const vec3s* v0, const vec3s* v1, const vec3s* v2,
                     vec3s* intersection_point, float* t) {
  const float EPSILON = 0.0000001f;

  vec3s edge1, edge2, h, s, q;
  float a, f, u, v;
  edge1 = glms_vec3_sub(*v1, *v0);
  edge2 = glms_vec3_sub(*v2, *v0);
  h = glms_vec3_cross(ray->dir, edge2);
  a = glms_vec3_dot(edge1, h);
  if (a > -EPSILON && a < EPSILON)
    return QB_FALSE;    // This ray is parallel to this triangle.
  f = 1.0f / a;
  s = glms_vec3_sub(ray->orig, *v0);
  u = f * glms_vec3_dot(s, h);
  if (u < 0.0f || u > 1.0f)
    return QB_FALSE;
  q = glms_vec3_cross(s, edge1);
  v = f * glms_vec3_dot(ray->dir, q);
  if (v < 0.0f || u + v > 1.0f)
    return QB_FALSE;
  // At this stage we can compute t to find out where the intersection point is on the line.
  *t = f * glms_vec3_dot(edge2, q);
  if (*t > EPSILON) { // ray intersection
    *intersection_point = glms_vec3_add(ray->orig, glms_vec3_scale(ray->dir, *t));
    return QB_TRUE;
  } else {
    // This means that there is a line intersection but not a ray intersection.
    return QB_FALSE;
  }
}

qbBool qb_ray_checkaabb(const qbCollider_* c, const qbTransform_* t, const qbRay_* r,
                      float* tmin_out, float* tmax_out) {
  vec3s max = glms_vec3_add(t->position, c->max);
  vec3s min = glms_vec3_add(t->position, c->min);

  float tmin = (min.x - r->orig.x) / r->dir.x;
  float tmax = (max.x - r->orig.x) / r->dir.x;

  if (tmin > tmax) std::swap(tmin, tmax);

  float tymin = (min.y - r->orig.y) / r->dir.y;
  float tymax = (max.y - r->orig.y) / r->dir.y;

  if (tymin > tymax) std::swap(tymin, tymax);

  if ((tmin > tymax) || (tymin > tmax))
    return QB_FALSE;

  if (tymin > tmin)
    tmin = tymin;

  if (tymax < tmax)
    tmax = tymax;

  float tzmin = (min.z - r->orig.z) / r->dir.z;
  float tzmax = (max.z - r->orig.z) / r->dir.z;

  if (tzmin > tzmax) std::swap(tzmin, tzmax);

  if ((tmin > tzmax) || (tzmin > tmax))
    return QB_FALSE;

  if (tzmin > tmin)
    tmin = tzmin;

  if (tzmax < tmax)
    tmax = tzmax;

  if (tmin_out) {
    *tmin_out = tmin;
  }
  if (tmax_out) {
    *tmax_out = tmax;
  }
  return QB_TRUE;
}

qbBool qb_ray_checkobb(const qbCollider_* c, const qbTransform_* t, const qbRay_* r,
                     float* tmin, float* tmax) {
  qbRay_ local_ray{};
  mat4s inv_orientation = glms_mat4_inv(t->orientation);
  local_ray.dir = glms_mat4_mulv3(inv_orientation, r->dir, 1.f);
  local_ray.orig = glms_mat4_mulv3(inv_orientation, r->orig, 0.f);

  return qb_ray_checkaabb(c, t, &local_ray, tmin, tmax);
}

qbResult qb_collider_frommesh(qbCollider* collider, qbMesh mesh) {
  *collider = MeshBuilder::Collider(mesh);
  return QB_OK;
}

qbBool qb_collider_mpr(const qbCollider_* a, const qbCollider_* b,
                       const qbTransform_* a_transform, const qbTransform_* b_transform) {
  const static float kEpsilon = 0.0000001f;

  MinkowskiDifference m = { a, b, a_transform, b_transform };

  auto mpr_phase_one = [&m](qbPortal_* out) {
    vec3s v = glms_vec3_sub(glms_vec3_add(m.a_transform->position, m.a->center),
                            glms_vec3_add(m.b_transform->position, m.b->center));
    vec3s a = glms_vec3_sub(pos_to_world(qb_collider_support(m.a, m.a_transform, glms_vec3_negate(v)), m.a_transform),
                            pos_to_world(qb_collider_support(m.b, m.b_transform, v), m.b_transform));
    
    vec3s v_a = glms_vec3_cross(v, a);
    if (glms_vec3_eq_eps(v_a, 0.f)) {
      if (glms_vec3_dot(v, a) < 0.f) {
        return 1;
      } else {
        return -1;
      }
    }

    vec3s b = glms_vec3_sub(pos_to_world(qb_collider_support(m.a, m.a_transform, v_a), m.a_transform),
                            pos_to_world(qb_collider_support(m.b, m.b_transform, glms_vec3_negate(v_a)), m.b_transform));
    
    vec3s avb = glms_vec3_cross(glms_vec3_sub(a, v), glms_vec3_sub(b, v));
    if (glms_vec3_eq_eps(avb, 0.0f)) {
      return -1;
    }

    vec3s c = glms_vec3_sub(pos_to_world(qb_collider_support(m.a, m.a_transform, avb), m.a_transform),
                            pos_to_world(qb_collider_support(m.b, m.b_transform, glms_vec3_negate(avb)), m.b_transform));
    out->v0 = v;
    out->v1 = a;
    out->v2 = b;
    out->v3 = c;
    return 0;
  };

  auto mpr_phase_two = [&m](qbPortal_* p) {
    vec3s r = glms_vec3_negate(p->v0);

    bool done;
    do {
      done = true;
      vec3s n_vab = glms_vec3_normalize(glms_vec3_cross(glms_vec3_sub(p->v1, p->v0),
                                        glms_vec3_sub(p->v2, p->v0)));

      vec3s n_vbc = glms_vec3_normalize(glms_vec3_cross(glms_vec3_sub(p->v2, p->v0),
                                        glms_vec3_sub(p->v3, p->v0)));

      vec3s n_vca = glms_vec3_normalize(glms_vec3_cross(glms_vec3_sub(p->v3, p->v0),
                                        glms_vec3_sub(p->v1, p->v0)));

      if (glms_vec3_dot(r, n_vab) > 0.f) {
        done = false;
        p->v3 = glms_vec3_sub(pos_to_world(qb_collider_support(m.a, m.a_transform, n_vab), m.a_transform),
                              pos_to_world(qb_collider_support(m.b, m.b_transform, glms_vec3_negate(n_vab)), m.b_transform));
        vec3s tmp = p->v1;
        p->v1 = p->v2;
        p->v2 = tmp;
      } else if (glms_vec3_dot(r, n_vbc) > 0.f) {
        done = false;
        p->v1 = glms_vec3_sub(pos_to_world(qb_collider_support(m.a, m.a_transform, n_vbc), m.a_transform),
                              pos_to_world(qb_collider_support(m.b, m.b_transform, glms_vec3_negate(n_vbc)), m.b_transform));
        vec3s tmp = p->v2;
        p->v2 = p->v3;
        p->v3 = tmp;
      } else if (glms_vec3_dot(r, n_vca) > 0.f) {
        done = false;
        p->v2 = glms_vec3_sub(pos_to_world(qb_collider_support(m.a, m.a_transform, n_vca), m.a_transform),
                              pos_to_world(qb_collider_support(m.b, m.b_transform, glms_vec3_negate(n_vca)), m.b_transform));
        vec3s tmp = p->v3;
        p->v3 = p->v1;
        p->v1 = tmp;
      }
    } while (!done);
  };

  auto mpr_phase_three = [&m](qbPortal_* p) {
    for (int i = 0; i < 1000; ++i) {
      vec3s n = glms_vec3_normalize(glms_vec3_cross(glms_vec3_sub(p->v3, p->v1),
                                    glms_vec3_sub(p->v2, p->v1)));
      if (glms_vec3_dot(p->v1, n) > 0.f) {
        return true;
      }

      if (std::abs(glms_vec3_dot(p->v1, n)) <= 0.0000001f) {
        return false;
      }

      vec3s p_ = glms_vec3_sub(pos_to_world(qb_collider_support(m.a, m.a_transform, n), m.a_transform),
                               pos_to_world(qb_collider_support(m.b, m.b_transform, glms_vec3_negate(n)), m.b_transform));

      if (glms_vec3_dot(p_, n) < 0.f) {
        return false;
      }

      if (glms_vec3_dot(p->v0, glms_vec3_cross(p_, p->v1)) > 0.f) {
        if (glms_vec3_dot(p->v0, glms_vec3_cross(p_, p->v2)) < 0.f) {
          p->v3 = p_;
        } else {
          p->v1 = p_;
        }
      } else {
        if (glms_vec3_dot(p->v0, glms_vec3_cross(p_, p->v3)) > 0.f) {
          p->v2 = p_;
        } else {
          p->v1 = p_;
        }
      }
    }
    return false;
  };

  qbPortal_ portal;
  int ray_case = mpr_phase_one(&portal);
  if (ray_case == 1) {
    return QB_TRUE;
  } else if (ray_case == -1) {
    return QB_FALSE;
  }

  mpr_phase_two(&portal);
  return mpr_phase_three(&portal);
}

qbBool qb_collider_checkmesh(const qbCollider_* a, const qbCollider_* b,
                             const qbTransform_* a_t, const qbTransform_* b_t) {
  return qb_collider_mpr(a, b, a_t, b_t);
}

qbBool qb_collider_check(const qbCollider_* a, const qbCollider_* b,
                       const qbTransform_* a_t, const qbTransform_* b_t) {
  if (qb_collider_checkaabb(a, b, a_t, b_t)) {
    return qb_collider_checkmesh(a, b, a_t, b_t);
  }
  return QB_FALSE;
}

qbBool qb_ray_checkplane(const qbRay_* ray, const qbRay_* plane, float* t) {
  // assuming vectors are all normalized
  float denom = glms_vec3_dot(plane->dir, ray->dir);
  if (denom > 1e-6) {
    vec3s p0l0 = glms_vec3_sub(plane->orig, ray->orig);
    *t = glms_vec3_dot(p0l0, plane->dir) / denom;
    return (*t >= 0);
  }

  return QB_FALSE;
}

qbBool qb_collider_checkray(const qbCollider_* c, const qbTransform_* transform, const qbRay_* r, float max_dis, float* t) {  
  // Intersection of a Line and a Convex Hull of Points Cloud
  // Author: R. P. Koptelov
  // http://www.m-hikari.com/ams/ams-2013/ams-101-104-2013/koptelovAMS101-104-2013.pdf
  if (!qb_ray_checkaabb(c, transform, r, nullptr, nullptr)) {
    return QB_FALSE;
  }

  if (!c->vertices) {
    if (!c->support) {
      return QB_TRUE;
    }

    qbCollider_ ray_c;
    qbTransform_ ray_t = qb_transform_identity;
    ray_t.position = r->orig;
    qb_collider_ray(&ray_c, r->dir, max_dis);
    qb_collider_checkmesh(c, &ray_c, transform, &ray_t);
    return QB_TRUE;
  }

  const static float kDistanceEpsilon = 0.0001f;
  const static float kAngleEpsilon = 0.0001f;

  vec3s from = glms_vec3_normalize(r->dir);
  vec3s to = vec3s{ 0.f, 0.f, 1.f };
  float rotation_angle = (float)std::acos(glms_vec3_dot(from, to));

  mat4s rotation_mat = GLMS_MAT4_IDENTITY_INIT;
  if (std::abs(rotation_angle) >= kAngleEpsilon) {
    vec3s rotation_axis = glms_vec3_normalize(glms_vec3_cross(from, to));
    rotation_mat = glms_rotate_make(rotation_angle, rotation_axis);
  }
  vec3s translation_vec = glms_vec3_negate(r->orig);

  std::vector<vec3s> p_neg_v;
  std::vector<vec3s> p_pos_v;
  p_neg_v.reserve(c->vertex_count);
  p_pos_v.reserve(c->vertex_count);
  for (int i = 0; i < c->vertex_count; ++i) {
    vec3s p = glms_vec3_add(glms_mat4_mulv3(transform->orientation, c->vertices[i], 1.f), transform->position);
    p = glms_mat4_mulv3(rotation_mat, glms_vec3_add(translation_vec, p), 1.f);
    if (p.y >= 0) {
      p_pos_v.push_back(p);
    } else {
      p_neg_v.push_back(p);
    }
  }

  if (p_neg_v.empty() || p_pos_v.empty()) {
    return QB_FALSE;
  }

  std::vector<vec3s> p_zero_v;
  p_zero_v.reserve(p_neg_v.size() * p_pos_v.size());
  for (size_t i = 0; i < p_neg_v.size(); ++i) {
    const auto& p_neg = p_neg_v[i];
    for (size_t j = i; j < p_pos_v.size(); ++j) {
      const auto& p_pos = p_pos_v[j];

      qbRay_ r{ p_neg, glms_vec3_normalize(glms_vec3_sub(p_pos, p_neg)) };
      qbRay_ plane{ vec3s{}, vec3s{0.f, 1.f, 0.f} };
      float t = 0.f;
      qb_ray_checkplane(&r, &plane, &t);
      p_zero_v.push_back(glms_vec3_add(r.orig, glms_vec3_scale(r.dir, t)));
    }
  }

  p_neg_v.clear();
  p_pos_v.clear();
  p_neg_v.reserve(p_zero_v.size());
  p_pos_v.reserve(p_zero_v.size());
  for (int i = 0; i < p_zero_v.size(); ++i) {
    vec3s p = p_zero_v[i];
    if (p.x >= 0) {
      p_pos_v.push_back(p);
    } else {
      p_neg_v.push_back(p);
    }
  }

  if (p_neg_v.empty() || p_pos_v.empty()) {
    return QB_FALSE;
  }

  p_zero_v.clear();
  p_zero_v.reserve(p_neg_v.size() * p_pos_v.size());
  for (size_t i = 0; i < p_neg_v.size(); ++i) {
    const auto& p_neg = p_neg_v[i];
    for (size_t j = i; j < p_pos_v.size(); ++j) {
      const auto& p_pos = p_pos_v[j];

      qbRay_ r{ p_neg, glms_vec3_normalize(glms_vec3_sub(p_pos, p_neg)) };
      qbRay_ plane{ vec3s{}, vec3s{ 1.f, 0.f, 0.f } };
      float t = 0.f;
      qb_ray_checkplane(&r, &plane, &t);
      p_zero_v.push_back(glms_vec3_add(r.orig, glms_vec3_scale(r.dir, t)));
    }
  }

  std::sort(p_zero_v.begin(), p_zero_v.end(), [](const vec3s& a, const vec3s& b) {
    return a.z < b.z;
  });

  *t = p_zero_v.front().z;
  return QB_TRUE;
}

void qb_collider_sphere(qbCollider collider, float r) {
  *collider = {};
  collider->max = {  r,  r,  r };
  collider->min = { -r, -r, -r };
  collider->r = r;
  collider->support = [](const qbCollider_* self, const vec3s dir) {
    return glms_vec3_scale(dir, self->r);
  };
}

template <typename T> int sgn(T val) {
  return (T(0) < val) - (val < T(0));
}

// http://uu.diva-portal.org/smash/get/diva2:343820/FULLTEXT01.pdf
void qb_collider_aabb(qbCollider collider, vec3s max, vec3s min, vec3s center) {
  *collider = {};
  collider->max = max;
  collider->min = min;
  collider->center = center;
  collider->r = std::max(glms_vec3_norm(glms_vec3_sub(max, center)),
                         glms_vec3_norm(glms_vec3_sub(min, center)));
  collider->support = [](const qbCollider_* self, const vec3s dir) {
    return vec3s{
      dir.x < 0 ? self->min.x : self->max.x,
      dir.y < 0 ? self->min.y : self->max.y,
      dir.z < 0 ? self->min.z : self->max.z,
    };
  };
}

void qb_collider_line(qbCollider collider, vec3s from, vec3s to) {
  *collider = {};
  collider->max = to;
  collider->min = from;
  collider->center = GLMS_VEC3_ZERO_INIT;

  float from_len = glms_vec3_norm(from);
  float to_len = glms_vec3_norm(to);
  collider->r = std::max(to_len, from_len);
  collider->support = [](const qbCollider_* self, const vec3s dir) {
    vec3s r = glms_vec3_sub(self->max, self->min);

    // If the segment and the given `dir` are pointing in the same direction,
    // then return the max otherwise min.
    return glms_vec3_dot(r, dir) < 0 ? self->min : self->max;
  };
}

void qb_collider_ray(qbCollider collider, vec3s dir, float t) {
  *collider = {};
  collider->max = { t, t, t };
  collider->min = { -t, -t, -t };
  collider->center = GLMS_VEC3_ZERO_INIT;
  collider->r = t;
  collider->support = [](const qbCollider_* self, const vec3s dir) {
    // If the segment and the given `dir` are pointing in the same direction,
    // then return the max otherwise min.
    return glms_vec3_dot(self->max, dir) <= 0 ? self->min : self->max;
  };
}

void qb_collider_pill(qbCollider collider, float r, float h) {
  *collider = {};
}

void qb_collider_cylinder(qbCollider collider, float r, float h) {
  *collider = {};
}

void qb_collider_cone(qbCollider collider, float r, float h) {
  *collider = {};
}

qbTransform_ qb_transform_identity = {
  .orientation = GLMS_MAT4_IDENTITY_INIT,
  .position = GLMS_VEC3_ZERO_INIT,
  .scale = GLMS_VEC3_ONE_INIT,
};