#ifndef MESH__H
#define MESH__H

#include <cubez/cubez.h>
#include <cubez/render_pipeline.h>

#include <vector>

typedef struct qbMaterial_* qbMaterial;
typedef struct qbShader_* qbShader;
typedef struct qbTexture_* qbTexture;

typedef struct qbVertex_ {
  vec3s p;
  vec3s n;
  vec2s t;
} qbVertex_, *qbVertex;

enum qbRenderFaceType_ {
  QB_POINTS,
  QB_LINES,
  QB_TRIANGLES,
  QB_TRIANGLE_LIST,
  QB_TRIANGLE_FAN,
};

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
  size_t image_count;

  qbGpuBuffer* uniforms;
  uint32_t* uniform_bindings;
  size_t uniform_count;
} qbMaterialAttr_, *qbMaterialAttr;

typedef struct qbMaterial_ {
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
  size_t image_count;

  qbGpuBuffer* uniforms;
  uint32_t* uniform_bindings;
  size_t uniform_count;
} qbMaterial_, *qbMaterial;

typedef struct qbCollider_ {
  vec3s* vertices;
  uint8_t count;

  vec3s max;
  vec3s min;
  vec3s r;
} qbCollider_, *qbCollider;

typedef struct qbMesh_ {
  vec3s* vertices;
  size_t vertex_count;

  vec3s* normals;
  size_t normal_count;

  vec2s* uvs;
  size_t uv_count;

  uint32_t* indices;
  size_t index_count;
} qbMesh_, *qbMesh;

// In memory representation.
typedef struct qbModel_ {
  const char* name;

  qbMesh meshes;
  size_t mesh_count;

  qbCollider colliders;
  size_t collider_count;
} qbModel_, *qbModel;

QB_API struct qbRenderable_* qb_model_load(const char* model_name, const char* filename);
QB_API void qb_model_destroy(qbModel* model);
QB_API bool qb_model_collides(vec3s a_origin, vec3s b_origin, qbModel a, qbModel b);

QB_API vec3s qb_collider_farthest(const qbCollider_* collider, vec3s dir);

QB_API qbResult qb_mesh_tobuffer(qbMesh mesh, qbMeshBuffer* buffer);

QB_API qbResult qb_material_create(qbMaterial* material, qbMaterialAttr attr, const char* material_name);
QB_API qbResult qb_material_destroy(qbMaterial* material);

QB_API struct qbRenderable_* qb_draw_cube(int size_x, int size_y, int size_z);
QB_API struct qbRenderable_* qb_draw_rect(int w, int h);
QB_API struct qbRenderable_* qb_draw_sphere(float radius, int slices, int zslices);

#endif   // MESH__H
