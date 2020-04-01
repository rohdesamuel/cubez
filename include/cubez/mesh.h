#ifndef MESH__H
#define MESH__H

#include <cubez/cubez.h>
#include <cubez/render_pipeline.h>

#include <glm/glm.hpp>
#include <vector>

typedef struct qbMaterial_* qbMaterial;
typedef struct qbShader_* qbShader;
typedef struct qbTexture_* qbTexture;

typedef struct qbVertex_ {
  glm::vec3 p;
  glm::vec3 n;
  glm::vec2 t;
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

  glm::vec3 albedo;
  float metallic;
  float roughness;
  glm::vec3 emission;

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

  glm::vec3 albedo;
  float metallic;
  float roughness;
  glm::vec3 emission;

  qbImage* images;
  uint32_t* image_units;
  size_t image_count;

  qbGpuBuffer* uniforms;
  uint32_t* uniform_bindings;
  size_t uniform_count;
} qbMaterial_, *qbMaterial;

typedef struct qbCollider_ {
  glm::vec3* vertices;
  uint8_t count;

  glm::vec3 max;
  glm::vec3 min;
  glm::vec3 r;
} qbCollider_, *qbCollider;

typedef struct qbMesh_ {
  glm::vec3* vertices;
  size_t vertex_count;

  glm::vec3* normals;
  size_t normal_count;

  glm::vec2* uvs;
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

QB_API qbResult qb_model_load(struct qbRenderable_** renderable, struct qbModel_** model,
                              const char* name, const char* file);
QB_API void qb_model_destroy(qbModel* model);
QB_API bool qb_model_collides(glm::vec3* a_origin, glm::vec3* b_origin, qbModel a, qbModel b);
QB_API void qb_model_upload(qbModel model, struct qbRenderable_** renderable);

QB_API glm::vec3 qb_collider_farthest(const qbCollider_* collider, glm::vec3 dir);

QB_API qbResult qb_mesh_load(qbMesh* mesh, const char* mesh_name,
                             const char* filename);
QB_API qbResult qb_mesh_find(qbMesh mesh, const char* mesh_name);
QB_API qbResult qb_mesh_tobuffer(qbMesh mesh, qbMeshBuffer* buffer);
QB_API qbResult qb_mesh_destroy(qbMesh* mesh);

QB_API qbResult qb_material_create(qbMaterial* material, qbMaterialAttr attr, const char* material_name);
QB_API qbResult qb_material_destroy(qbMaterial* material);
QB_API qbMaterial qb_material_find(const char* name);

QB_API struct qbRenderable_* qb_draw_cube(int size_x, int size_y, int size_z);
QB_API struct qbRenderable_* qb_draw_rect(int w, int h);
QB_API struct qbRenderable_* qb_draw_sphere(float radius, int slices, int zslices);

#endif   // MESH__H
