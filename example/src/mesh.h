#ifndef MESH__H
#define MESH__H

#include <cubez/cubez.h>

#include <glm/glm.hpp>
#include <vector>

struct qbMeshAttributes_ {
  std::vector<glm::vec3> v;
  std::vector<glm::vec3> vn;
  std::vector<glm::vec2> vt;
  std::vector<uint32_t> indices;
};

enum qbRenderMode {
  QB_POINTS,
  QB_LINES,
  QB_TRIANGLES
};

struct qbMesh_ {
  uint32_t vao;
  uint32_t el_vbo;
  uint32_t v_vbo;
  uint32_t vn_vbo;
  uint32_t vt_vbo;
  
  qbRenderMode render_mode;
  qbMeshAttributes_* attributes;
};

typedef struct qbMesh_* qbMesh;
typedef struct qbMaterialAttr_* qbMaterialAttr;
typedef struct qbMaterial_* qbMaterial;
typedef struct qbShader_* qbShader;
typedef struct qbTexture_* qbTexture;

qbResult qb_mesh_load(qbMesh* mesh, const char* mesh_name,
                      const char* filename);
qbResult qb_mesh_find(qbMesh mesh, const char* mesh_name);
uint32_t qb_mesh_getvbuffer(qbMesh mesh);
uint32_t qb_mesh_getvtbuffer(qbMesh mesh);
uint32_t qb_mesh_getvnbuffer(qbMesh mesh);


qbResult qb_mesh_destroy();

qbResult qb_mesh_draw(qbMesh mesh, qbMaterial shader);

qbResult qb_shader_load(qbShader* shader, const char* shader_name,
                        const char* vs_filename, const char* fs_filename);
qbResult qb_shader_find(qbShader* shader, const char* shader_name);
qbId qb_shader_getid(qbShader shader);

qbResult qb_shader_setbool(qbShader shader, const char* uniform, bool value);
qbResult qb_shader_setint(qbShader shader, const char* uniform, int value);
qbResult qb_shader_setfloat(qbShader shader, const char* uniform, float value);
qbResult qb_shader_setvec2(qbShader shader, const char* uniform, glm::vec2 value);
qbResult qb_shader_setvec3(qbShader shader, const char* uniform, glm::vec3 value);
qbResult qb_shader_setvec4(qbShader shader, const char* uniform, glm::vec4 value);
qbResult qb_shader_setmat2(qbShader shader, const char* uniform, glm::mat2 value);
qbResult qb_shader_setmat3(qbShader shader, const char* uniform, glm::mat3 value);
qbResult qb_shader_setmat4(qbShader shader, const char* uniform, glm::mat4 value);

qbResult qb_shader_destroy(qbShader*);

qbResult qb_texture_load(qbTexture* texture, const char* texture_name,
                         const char* filename);
qbResult qb_texture_find(qbTexture* texture, const char* texture_name);
qbResult qb_texture_destroy(qbTexture* texture);

qbResult qb_materialattr_create(qbMaterialAttr* attr);
qbResult qb_materialattr_destroy(qbMaterialAttr* attr);
qbResult qb_materialattr_setshader(qbMaterialAttr attr, qbShader shader);
qbResult qb_materialattr_addtexture(qbMaterialAttr attr,
    qbTexture texture, glm::vec2 offset, glm::vec2 scale);
qbResult qb_materialattr_setcolor(qbMaterialAttr attr, glm::vec4 color);

qbResult qb_material_create(qbMaterial* material, qbMaterialAttr attr);
qbResult qb_material_setcolor(qbMaterial attr, glm::vec4 color);
qbResult qb_material_find(qbMaterial* material, const char* material_name);
qbResult qb_material_use(qbMaterial material);
qbShader qb_material_getshader(qbMaterial material);
qbResult qb_material_destroy(qbMaterial* material);

#endif   // MESH__H
