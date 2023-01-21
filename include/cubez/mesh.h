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
  vec3s albedo;
  float metallic;
  vec3s emission;
  float roughness;  

  qbImage albedo_map;
  qbImage normal_map;
  qbImage metallic_map;
  qbImage roughness_map;
  qbImage ao_map;
  qbImage emission_map;

  const char* name;

  qbRenderExt ext;
} qbMaterial_, *qbMaterial;

typedef struct qbCollider_ {
  vec3s* vertices;
  uint8_t vertex_count;
  vec3s center;

  vec3s max;
  vec3s min;  
  float r;

  vec3s(*support)(const qbCollider_* self, const vec3s* dir, const qbTransform_* transform);
} qbCollider_, *qbCollider;

typedef struct qbMesh_ {
  qbGpuBuffer vbo;
  qbGpuBuffer ibo;

  vec3s* vertices;
  vec3s* normals;
  vec2s* uvs;
  uint32_t* indices;

  uint32_t vertex_count;
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

QB_API qbMesh qb_mesh_load(const char* mesh_name, const char* filename);


// Old-API
// TODO: remove unused

QB_API qbModel qb_model_load(const char* model_name, const char* filename);

QB_API void qb_model_create(qbModel* model, qbModelAttr attr);
QB_API void qb_model_destroy(qbModel* model);

QB_API qbResult qb_mesh_destroy(qbMesh* mesh);
QB_API qbResult qb_collider_destroy(qbCollider* collider);

QB_API qbResult qb_material_create(qbMaterial* material, qbMaterialAttr attr, const char* material_name);
QB_API qbResult qb_material_destroy(qbMaterial* material);

QB_API qbBool qb_collider_check(const qbCollider_* a, const qbCollider_* b,
                              const qbTransform_* a_t, const qbTransform_* b_t);

QB_API qbBool qb_collider_checkaabb(const qbCollider_* a, const qbCollider_* b,
                                  const qbTransform_* a_t, const qbTransform_* b_t);

QB_API qbBool qb_collider_checkmesh(const qbCollider_* a, const qbCollider_* b,
                                  const qbTransform_* a_t, const qbTransform_* b_t);

QB_API qbBool qb_collider_checkray(const qbCollider_* collider, const qbTransform_* transform, const qbRay_* r, float* t);

QB_API vec3s qb_collider_support(const qbCollider_* collider, const qbTransform_* transform, vec3s dir);

// Unimplemented.
QB_API void qb_collider_sphere(qbCollider collider, float r);

// Unimplemented.
QB_API void qb_collider_aabb(qbCollider collider, vec3s max, vec3s min, vec3s center);

// Unimplemented.
QB_API void qb_collider_obb(qbCollider collider, vec3s max, vec3s min, vec3s center);

// Unimplemented.
QB_API void qb_collider_pill(qbCollider collider, float r, float h);

// Unimplemented.
QB_API void qb_collider_cylinder(qbCollider collider, float r, float h);

// Unimplemented.
QB_API void qb_collider_cone(qbCollider collider, float r, float h);

QB_API qbBool qb_ray_checkaabb(const qbCollider_* c, const qbTransform_* t, const qbRay_* r,
                             float* tmin, float* tmax);

// Unimplemented.
QB_API qbBool qb_ray_checkobb(const qbCollider_* c, const qbTransform_* t, const qbRay_* r,
                            float* tmin, float* tmax);

QB_API qbBool qb_ray_checktri(const qbRay_* ray, const vec3s* v0, const vec3s* v1, const vec3s* v2,
                            vec3s* intersection_point, float* t);

QB_API qbBool qb_ray_checkplane(const qbRay_* ray, const qbRay_* plane, float* t);

#endif   // MESH__H
