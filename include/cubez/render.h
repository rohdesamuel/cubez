#ifndef CUBEZ_RENDER__H
#define CUBEZ_RENDER__H

#include <cubez/cubez.h>
#include <cubez/render_pipeline.h>
#include <glm/glm.hpp>

// TODO: Add a 2D "surface" that you can draw to.

/*

GLSL Keywords:
  qb_texture_unit_1 .. qb_texture_unit_N

*/

typedef struct qbRenderable_* qbRenderable;

typedef struct qbRenderer_ {
  void(*render)(struct qbRenderer_* self, const struct qbCamera_* camera, qbRenderEvent event);
  
  void(*rendergroup_oncreate)(struct qbRenderer_* self, qbRenderGroup);
  void(*rendergroup_ondestroy)(struct qbRenderer_* self, qbRenderGroup);

  // Thread-safe
  // Adds the given model to the background qbRenderPipeline.
  void(*rendergroup_add)(struct qbRenderer_* self, qbRenderGroup model);

  // Thread-safe
  // Removes the given model from the background qbRenderPipeline.
  void(*rendergroup_remove)(struct qbRenderer_* self, qbRenderGroup model);

  // Reserved
  void(*surface_add)(struct qbRenderer_* self, struct qbSurface_*);
  void(*surface_remove)(struct qbRenderer_* self, struct qbSurface_*);

  size_t(*max_texture_units)(struct qbRenderer_* self);
  size_t(*max_uniform_units)(struct qbRenderer_* self);
  size_t(*max_lights)(struct qbRenderer_* self);

  qbMeshBuffer(*meshbuffer_create)(struct qbRenderer_* self, struct qbMesh_* mesh);
  void(*meshbuffer_attach_material)(struct qbRenderer_* self, qbMeshBuffer buffer,
                                    struct qbMaterial_* material);
  void(*meshbuffer_attach_textures)(struct qbRenderer_* self, qbMeshBuffer buffer,
                                    size_t count,
                                    uint32_t texture_units[],
                                    qbImage textures[]);
  void(*meshbuffer_attach_uniforms)(struct qbRenderer_* self, qbMeshBuffer buffer,
                                    size_t count,
                                    uint32_t uniform_bindings[],
                                    qbGpuBuffer uniforms[]);
  void(*rendergroup_attach_material)(struct qbRenderer_* self, qbRenderGroup group,
                                     struct qbMaterial_* material);
  void(*rendergroup_attach_textures)(struct qbRenderer_* self, qbRenderGroup group,
                                     size_t count,
                                     uint32_t texture_units[],
                                     qbImage textures[]);
  void(*rendergroup_attach_uniforms)(struct qbRenderer_* self, qbRenderGroup group,
                                     size_t count,
                                     uint32_t uniform_bindings[],
                                     qbGpuBuffer uniforms[]);

  void(*light_add)(struct qbRenderer_* self, qbId id);
  void(*light_remove)(struct qbRenderer_* self, qbId id);
  void(*light_enable)(struct qbRenderer_* self, qbId id);
  void(*light_disable)(struct qbRenderer_* self, qbId id);
  void(*light_directional)(struct qbRenderer_* self, qbId id, glm::vec3 rgb, glm::vec3 dir, float brightness);
  void(*light_point)(struct qbRenderer_* self, qbId id, glm::vec3 rgb, glm::vec3 pos, float brightness, float range);
  void(*light_spotlight)(struct qbRenderer_* self, qbId id, glm::vec3 rgb, glm::vec3 pos, glm::vec3 dir,
                                 float brightness, float range, float angle_deg);

  // TODO: This should be a surface that the underlying renderer draws to.
  void(*set_gui_renderpass)(struct qbRenderer_* self, qbRenderPass gui_renderpass);

  const char* title;
  int width;
  int height;
  qbRenderPipeline render_pipeline;

  void* state;
} qbRenderer_, *qbRenderer;

typedef struct qbCameraAttr_ {
  uint32_t width;
  uint32_t height;
  float fov;
  float znear;
  float zfar;

  glm::mat4 rotation_mat;
  glm::vec3 origin;
} qbCameraAttr_, *qbCameraAttr;

typedef struct qbCamera_ {
  uint32_t width;
  uint32_t height;
  float ratio;
  float fov;
  float znear;
  float zfar;

  glm::mat4 view_mat;
  glm::mat4 projection_mat;
  glm::mat4 rotation_mat;
  glm::vec3 origin;
  glm::vec3 up;
  glm::vec3 forward;
} qbCamera_;
typedef const qbCamera_* qbCamera;

typedef struct qbRenderEvent_ {
  double alpha;
  uint64_t frame;

  qbRenderer renderer;
  qbCamera camera;
} qbRenderEvent_, *qbRenderEvent;

enum qbLightType_ {
  QB_LIGHT_TYPE_DIRECTIONAL,
  QB_LIGHT_TYPE_SPOTLIGHT,
  QB_LIGHT_TYPE_POINT,
};

QB_API uint32_t qb_window_width();
QB_API uint32_t qb_window_height();
QB_API void qb_window_resize(uint32_t width, uint32_t height);

QB_API void qb_camera_create(qbCamera* camera, qbCameraAttr attr);
QB_API void qb_camera_destroy(qbCamera* camera);
QB_API void qb_camera_activate(qbCamera camera);
QB_API void qb_camera_deactivate(qbCamera camera);
QB_API qbCamera qb_camera_active();

QB_API void qb_camera_resize(qbCamera camera, uint32_t width, uint32_t height);
QB_API void qb_camera_fov(qbCamera camera, float fov);
QB_API void qb_camera_clip(qbCamera camera, float znear, float zfar);
QB_API void qb_camera_rotation(qbCamera camera, glm::mat4 rotation);
QB_API void qb_camera_origin(qbCamera camera, glm::vec3 origin);
QB_API qbFrameBuffer qb_camera_fbo(qbCamera camera);

QB_API glm::vec3 qb_camera_screentoworld(qbCamera camera, glm::vec2 screen);
QB_API glm::vec2 qb_camera_worldtoscreen(qbCamera camera, glm::vec3 world);

QB_API qbId qb_light_add();
QB_API void qb_light_remove(qbId id);
QB_API void qb_light_enable(qbId id);
QB_API void qb_light_disable(qbId id);
QB_API bool qb_light_isenabled(qbId);
QB_API void qb_light_directional(qbId id, glm::vec3 rgb, glm::vec3 dir, float brightness);
QB_API void qb_light_point(qbId id, glm::vec3 rgb, glm::vec3 pos, float brightness, float range);
QB_API void qb_light_spotlight(qbId id, glm::vec3 rgb, glm::vec3 pos, glm::vec3 dir,
                               float brightness, float range, float angle_deg);
QB_API size_t qb_light_max();
QB_API size_t qb_light_count();


QB_API qbResult qb_render(qbRenderEvent event);
QB_API qbEvent qb_render_event();

QB_API qbRenderer qb_renderer();
QB_API qbResult qb_render_swapbuffers();
QB_API qbResult qb_render_makecurrent();
QB_API qbResult qb_render_makenull();

typedef struct qbTransform_ {
  glm::vec3 pivot;
  glm::vec3 position;
  glm::mat4 orientation;
} qbTransform_, *qbTransform;

QB_API void qb_renderable_create(qbRenderable* renderable, struct qbModel_* model);
QB_API void qb_renderable_destroy(qbRenderable* renderable);

// Frees from GPU.
QB_API void qb_renderable_free(qbRenderable renderable);

// Updates model in RAM.
QB_API void qb_renderable_update(qbRenderable renderable, struct qbModel_* model);

// Uploads model and material to GPU.
QB_API void qb_renderable_upload(qbRenderable renderable, struct qbMaterial_* material);

QB_API struct qbModel_* qb_renderable_model(qbRenderable renderable);
QB_API qbRenderGroup qb_renderable_rendergroup(qbRenderable renderable);

QB_API qbComponent qb_renderable();
QB_API qbComponent qb_material();
QB_API qbComponent qb_transform();

#endif  // CUBEZ_RENDER__H
