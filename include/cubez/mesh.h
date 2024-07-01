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

typedef struct qbTransform_ {
  mat4s orientation;
  vec3s position;
  vec3s scale;
} qbTransform_, * qbTransform;

typedef struct qbMaterialImageLoadAttr_ {
  const utf8_t* name;
  qbImageAttr_ image_load_attr;
} qbMaterialImageLoadAttr_, *qbMaterialImageLoadAttr;

typedef struct qbMaterialAttr_ {
  qbImage color_map;
  qbImage ambient_map;
  qbImage specular_map;
  qbImage emissive_map;

  // PBR-named material maps.
  qbImage normal_map;
  qbImage metallic_map;
  qbImage roughness_map;
  qbImage shininess_map;
  qbImage ao_map;

  vec3s color;
  vec3s ambient;
  vec3s specular;
  vec3s emissive;

  float metallic;
  float roughness;
  float shininess;

  qbRenderExt ext;
} qbMaterialAttr_, *qbMaterialAttr;

typedef struct qbMaterial_ {
  const char* name;

  qbImage color_map;
  qbImage ambient_map;
  qbImage specular_map;
  qbImage emissive_map;

  // PBR-named material maps.
  qbImage normal_map;
  qbImage metallic_map;
  qbImage roughness_map;
  qbImage shininess_map;
  qbImage ao_map;

  vec3s color;
  vec3s ambient;
  vec3s specular;
  vec3s emissive;

  float metallic;
  float roughness;
  float shininess;

  qbRenderExt ext;
} qbMaterial_, *qbMaterial;

typedef struct qbCollider_ {
  vec3s* vertices;
  uint8_t vertex_count;
  vec3s center;

  vec3s max;
  vec3s min;  
  float r;

  vec3s(*support)(const qbCollider_* self, const vec3s dir);
} qbCollider_, *qbCollider;

typedef struct qbMeshFace_ {
  uint32_t* indices;
  uint32_t index_count;
} qbMeshFace_;

typedef struct qbMesh_ {
  // Required. Contains `vertex_count` number of elements.
  vec3s* vertices;

  // Optional. Contains `vertex_count` number of elements.
  vec3s* normals;

  // Optional. Contains `vertex_count` number of elements.
  vec2s* uvs;

  // Optional. Contains `vertex_count * color_channels` number of elements.
  float* colors;

  // Optional. Contains `index_count` number of elements.
  uint32_t* indices;

  uint32_t vertex_count;
  uint32_t index_count;
  int color_channels;

  qbDrawMode mode;
} qbMesh_, *qbMesh;

typedef struct qbRenderable_ {
  qbGpuBuffer* vbos;
  size_t vbo_count;

  qbGpuBuffer ibo;

  uint32_t first_vbo_binding;
  uint32_t binding_count;

  uint32_t vertex_offset;
  uint32_t vertex_count;
  uint32_t index_count;
} qbRenderable_, *qbRenderable;

typedef struct qbRay_ {
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

  qbMaterial* materials;
  uint32_t material_count;

  uint32_t* material_bindings;
  uint32_t material_binding_count;

  qbDrawMode mode;
} qbModel_, *qbModel;

typedef struct qbModelAttr_ {
  const char* name;

  qbMesh* meshes;
  uint32_t mesh_count;

  qbCollider* colliders;
  uint32_t collider_count;

  qbMaterial* materials;
  uint32_t material_count;

  uint32_t* material_bindings;
  uint32_t material_binding_count;
} qbModelAttr_, *qbModelAttr;

typedef struct qbMeshBuilder_* qbMeshBuilder;
typedef struct qbMeshBuilderAttr_ {
  int color_channels;
} qbMeshBuilderAttr_, *qbMeshBuilderAttr;

QB_API qbResult qb_meshbuilder_create(qbMeshBuilder* builder, qbMeshBuilderAttr attr);
QB_API qbResult qb_meshbuilder_destroy(qbMeshBuilder* builder);
QB_API void qb_meshbuilder_clear(qbMeshBuilder builder);
QB_API qbResult qb_meshbuilder_build(qbMeshBuilder builder, qbDrawMode mode, qbMesh* mesh, qbCollider* collider);
QB_API int qb_meshbuilder_addv(qbMeshBuilder builder, vec3s v);
QB_API int qb_meshbuilder_addvo(qbMeshBuilder builder, vec3s v, vec3s o);
QB_API int qb_meshbuilder_addvt(qbMeshBuilder builder, vec2s vt);
QB_API int qb_meshbuilder_addvn(qbMeshBuilder builder, vec3s vn);
QB_API int qb_meshbuilder_addvc(qbMeshBuilder builder, float color[]);
QB_API int qb_meshbuilder_addline(qbMeshBuilder builder, int vertices[], int normals[], int uvs[], int cols[]);
QB_API int qb_meshbuilder_addtri(qbMeshBuilder builder, int vertices[], int normals[], int uvs[], int cols[]);

QB_API qbModel qb_model_load(const char* model_name, const utf8_t* filename);
QB_API qbMesh qb_mesh_load(const char* mesh_name, const utf8_t* filename);

// Old-API
// TODO: remove unused


QB_API void qb_model_create(qbModel* model, qbModelAttr attr);
QB_API void qb_model_destroy(qbModel* model);

QB_API qbResult qb_mesh_destroy(qbMesh* mesh);
QB_API qbResult qb_collider_destroy(qbCollider* collider);

QB_API qbResult qb_material_create(qbMaterial* material, qbMaterialAttr attr, const char* material_name);
QB_API qbResult qb_material_destroy(qbMaterial* material);

QB_API qbResult qb_collider_frommesh(qbCollider* collider, qbMesh mesh);

QB_API qbBool qb_collider_check(const qbCollider_* a, const qbCollider_* b,
                              const qbTransform_* a_t, const qbTransform_* b_t);

QB_API qbBool qb_collider_checkaabb(const qbCollider_* a, const qbCollider_* b,
                                  const qbTransform_* a_t, const qbTransform_* b_t);

QB_API qbBool qb_collider_checkmesh(const qbCollider_* a, const qbCollider_* b,
                                  const qbTransform_* a_t, const qbTransform_* b_t);

QB_API qbBool qb_collider_checkray(const qbCollider_* collider, const qbTransform_* transform, const qbRay_* r, float max_dis, float* t);

// Returns the vertex rotated and scaled with the given transform (model-space)
// farthest away in the given direction. Returns a vertex from `vertices`, if
// set. Otherwise calls the collider's support function.
QB_API vec3s qb_collider_support(const qbCollider_* collider, const qbTransform_* transform, vec3s dir);

// Fills `collider` with a sphere collider centered at the origin.
QB_API void qb_collider_sphere(qbCollider collider, float r);

// Fills `collider` with a AABB collider centered at the origin.
QB_API void qb_collider_aabb(qbCollider collider, vec3s max, vec3s min, vec3s center);

// Fills `collider` with a line-segment collider centered at the origin.
QB_API void qb_collider_line(qbCollider collider, vec3s from, vec3s to);

// Fills `collider` with a ray collider centered at the origin and length `t`.
QB_API void qb_collider_ray(qbCollider collider, vec3s dir, float t);

// Unimplemented.
QB_API void qb_collider_pill(qbCollider collider, float r, float h);

// Unimplemented.
QB_API void qb_collider_cylinder(qbCollider collider, float r, float h);

// Unimplemented.
QB_API void qb_collider_cone(qbCollider collider, float r, float h);

QB_API qbBool qb_ray_checkaabb(const qbCollider_* c, const qbTransform_* t, const qbRay_* r,
                             float* tmin, float* tmax);

QB_API qbBool qb_ray_checkobb(const qbCollider_* c, const qbTransform_* t, const qbRay_* r,
                            float* tmin, float* tmax);

QB_API qbBool qb_ray_checktri(const qbRay_* ray, const vec3s* v0, const vec3s* v1, const vec3s* v2,
                            vec3s* intersection_point, float* t);

QB_API qbBool qb_ray_checkplane(const qbRay_* ray, const qbRay_* plane, float* t);

QB_API qbTransform_ qb_transform_identity;

#endif   // MESH__H
