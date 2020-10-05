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

#ifndef MESH__H
#define MESH__H

#include <cubez/cubez.h>
#include <cubez/render_pipeline.h>

#include <vector>

typedef struct qbMaterial_* qbMaterial;
typedef struct qbShader_* qbShader;
typedef struct qbTexture_* qbTexture;
typedef struct qbTransform_* qbTransform;

typedef struct qbMaterialAttr_ {
  qbImage albedo_map;
  qbImage normal_map;
  qbImage metallic_map;
  qbImage roughness_map;
  qbImage ao_map;
  qbImage emission_map;

  vec3s albedo;
  float metallic;
  float roughness;
  vec3s emission;

  qbImage* images;
  uint32_t* image_units;
  uint32_t image_count;

  qbGpuBuffer* uniforms;
  uint32_t* uniform_bindings;
  uint32_t uniform_count;

  qbRenderExt ext;
} qbMaterialAttr_, *qbMaterialAttr;

typedef struct qbMaterial_ {
  const char* name;

  qbImage albedo_map;
  qbImage normal_map;
  qbImage metallic_map;
  qbImage roughness_map;
  qbImage ao_map;
  qbImage emission_map;

  vec3s albedo;
  float metallic;
  float roughness;
  vec3s emission;

  qbImage* images;
  uint32_t* image_units;
  uint32_t image_count;

  qbGpuBuffer* uniforms;
  uint32_t* uniform_bindings;
  uint32_t uniform_count;

  qbRenderExt ext;
} qbMaterial_, *qbMaterial;

typedef struct qbCollider_ {
  vec3s* vertices;
  uint8_t vertex_count;

  vec3s max;
  vec3s min;
  vec3s center;
  float r;
} qbCollider_, *qbCollider;

typedef struct qbMesh_ {
  vec3s* vertices;
  uint32_t vertex_count;

  vec3s* normals;
  uint32_t normal_count;

  vec2s* uvs;
  uint32_t uv_count;

  uint32_t* indices;
  uint32_t index_count;

  qbDrawMode mode;
} qbMesh_, *qbMesh;

typedef struct {
  vec3s orig;
  vec3s dir;
} qbRay_, *qbRay;

// In memory representation.
typedef struct qbModel_ {
  const char* name;

  qbMesh* meshes;
  uint32_t mesh_count;

  qbCollider* colliders;
  uint32_t collider_count;

  qbDrawMode mode;
} qbModel_, *qbModel;

// In memory representation.
typedef struct qbModelAttr_ {
  const char* name;

  qbMesh* meshes;
  uint32_t mesh_count;

  qbCollider* colliders;
  uint32_t collider_count;
} qbModelAttr_, *qbModelAttr;

typedef struct qbIntersection_ {
  vec3s n;
  float l;
} qbIntersection_, *qbIntersection;

typedef struct qbMeshBuilder_* qbMeshBuilder;
QB_API qbResult qb_meshbuilder_create(qbMeshBuilder* builder);
QB_API qbResult qb_meshbuilder_destroy(qbMeshBuilder* builder);
QB_API qbResult qb_meshbuilder_build(qbMeshBuilder builder, qbDrawMode mode, qbMesh* mesh, qbCollider* collider);
QB_API int qb_meshbuilder_addv(qbMeshBuilder builder, vec3s v);
QB_API int qb_meshbuilder_addvo(qbMeshBuilder builder, vec3s v, vec3s o);
QB_API int qb_meshbuilder_addvt(qbMeshBuilder builder, vec2s vt);
QB_API int qb_meshbuilder_addvn(qbMeshBuilder builder, vec3s vn);
QB_API int qb_meshbuilder_addline(qbMeshBuilder builder, int vertices[], int normals[], int uvs[]);
QB_API int qb_meshbuilder_addtri(qbMeshBuilder builder, int vertices[], int normals[], int uvs[]);

QB_API qbModel qb_model_load(const char* model_name, const char* filename);
QB_API struct qbModelGroup_* qb_model_upload(qbModel model);

QB_API void qb_model_create(qbModel* model, qbModelAttr attr);
QB_API void qb_model_destroy(qbModel* model);

QB_API qbResult qb_mesh_tobuffer(qbMesh mesh, qbMeshBuffer* buffer);
QB_API qbResult qb_mesh_destroy(qbMesh* mesh);
QB_API qbResult qb_collider_destroy(qbCollider* collider);

QB_API qbResult qb_material_create(qbMaterial* material, qbMaterialAttr attr, const char* material_name);
QB_API qbResult qb_material_destroy(qbMaterial* material);

QB_API struct qbModelGroup_* qb_draw_cube(float size_x, float size_y, float size_z, qbDrawMode mode, qbCollider* collider);
QB_API struct qbModelGroup_* qb_draw_rect(float w, float h, qbDrawMode mode, qbCollider* collider);
QB_API struct qbModelGroup_* qb_draw_sphere(float radius, int slices, int zslices, qbDrawMode mode, qbCollider* collider);

QB_API bool qb_collider_check(const qbCollider_* a, const qbCollider_* b,
                              const qbTransform_* a_t, const qbTransform_* b_t);

QB_API bool qb_collider_checkaabb(const qbCollider_* a, const qbCollider_* b,
                                  const qbTransform_* a_t, const qbTransform_* b_t);

QB_API bool qb_collider_checkmesh(const qbCollider_* a, const qbCollider_* b,
                                  const qbTransform_* a_t, const qbTransform_* b_t);

QB_API bool qb_collider_checkray(const qbCollider_* collider, const qbTransform_* transform, const qbRay_* r, float* t);

QB_API vec3s qb_collider_support(const qbCollider_* collider, const qbTransform_* transform, vec3s dir);

QB_API bool qb_ray_checktri(const qbRay_* ray, const vec3s* v0, const vec3s* v1, const vec3s* v2,
                            vec3s* intersection_point, float* dis);

QB_API bool qb_ray_checkplane(const qbRay_* ray, const qbRay_* plane, float* t);

#endif   // MESH__H
