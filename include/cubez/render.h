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

#ifndef CUBEZ_RENDER__H
#define CUBEZ_RENDER__H

#include <cubez/cubez.h>
#include <cubez/render_pipeline.h>
#include <cglm/cglm.h>
#include <cglm/types-struct.h>

typedef struct qbModelGroup_* qbModelGroup;

typedef struct qbRenderer_ {
  void(*render)(struct qbRenderer_* self, qbRenderEvent event);
  void(*resize)(struct qbRenderer_* self, uint32_t width, uint32_t height);
  qbResult(*drawcommands_submit)(struct qbRenderer_* self, struct qbDrawCommands_* cmds);
  
  size_t(*max_lights)(struct qbRenderer_* self);
  void(*light_enable)(struct qbRenderer_* self, qbId id, enum qbLightType type);
  void(*light_disable)(struct qbRenderer_* self, qbId id, enum qbLightType type);
  bool(*light_isenabled)(struct qbRenderer_* self, qbId id, enum qbLightType type);
  void(*light_directional)(struct qbRenderer_* self, qbId id, vec3s rgb,
    vec3s dir, float brightness);
  void(*light_point)(struct qbRenderer_* self, qbId id, vec3s rgb,
    vec3s pos, float linear, float quadratic, float radius);
  void(*light_spot)(struct qbRenderer_* self, qbId id, vec3s rgb,
    vec3s pos, vec3s dir, float brightness,
    float radius, float angle_deg);
  size_t(*light_max)(struct qbRenderer_* self, qbLightType light_type);

  void(*modelgroup_create)(struct qbRenderer_* self, qbModelGroup* modelgroup);
  void(*modelgroup_destroy)(struct qbRenderer_* self, qbModelGroup* modelgroup);
  void(*modelgroup_upload)(struct qbRenderer_* self, qbModelGroup modelgroup,
                           struct qbModel_* model, struct qbMaterial_* material);
  qbRenderGroup(*modelgroup_rendergroup)(struct qbRenderer_* self, qbModelGroup modelgroup);

  size_t(*max_texture_units)(struct qbRenderer_* self);
  size_t(*max_uniform_units)(struct qbRenderer_* self);  

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

  qbFrameBuffer(*camera_framebuffer_create)(struct qbRenderer_* self, uint32_t width, uint32_t height);

  const char* title;
  qbRenderPipeline render_pipeline;

  void* state;
} qbRenderer_, *qbRenderer;

typedef struct qbRendererAttr_ {
  struct qbRenderer_* (*create_renderer)(uint32_t width, uint32_t height, struct qbRendererAttr_* args);
  void(*destroy_renderer)(struct qbRenderer_* renderer);

  // A list of any new uniforms to be used in the shader. The bindings should
  // start at 0. These should not include any texture sampler uniforms. For
  // those, use the image_samplers value.
  qbShaderResourceBinding shader_resources;
  uint32_t shader_resource_count;

  // The bindings should start at 0. These should not include any texture
  // sampler uniforms. For those, use the image_samplers value.
  // Unimplemented.
  qbGpuBuffer* uniforms;
  uint32_t* uniform_bindings;
  uint32_t uniform_count;

  // A list of any new texture samplers to be used in the shader. This will
  // automatically create all necessary qbShaderResourceInfos. Do not create
  // individual qbShaderResourceInfos for the given samplers.
  qbImageSampler* image_samplers;
  uint32_t image_sampler_count;

  // An optional renderpass to draw the gui.
  qbRenderPass opt_gui_renderpass;

  // An optional present pass to draw the final frame.
  qbRenderPass opt_present_renderpass;

  // Optional arguments to pass to the create_renderer function.
  void* opt_args;

} qbRendererAttr_, *qbRendererAttr;

typedef struct qbCamera_ {
  float aspect;
  float near;
  float far;
  float fov;

  vec3s eye;
  mat4s view_mat;
  mat4s projection_mat;
} qbCamera_, *qbCamera;

typedef struct qbRenderEvent_ {
  double alpha;
  double dt;
  uint64_t frame;

  qbRenderer renderer;
} qbRenderEvent_, *qbRenderEvent;

enum qbLightType {
  QB_LIGHT_TYPE_DIRECTIONAL,
  QB_LIGHT_TYPE_SPOTLIGHT,
  QB_LIGHT_TYPE_POINT,
};

enum qbFullscreenType {
  QB_WINDOWED,
  QB_WINDOW_FULLSCREEN,
  QB_WINDOW_FULLSCREEN_DESKTOP
};

QB_API uint32_t qb_window_width();
QB_API uint32_t qb_window_height();

QB_API void qb_window_resize(uint32_t width, uint32_t height);
QB_API void qb_window_setfullscreen(qbFullscreenType type);
QB_API qbFullscreenType  qb_window_fullscreen();
QB_API void qb_window_setbordered(int bordered);
QB_API int qb_window_bordered();

QB_API qbCamera qb_camera_ortho(float left, float right, float bottom, float top, vec2s eye);
QB_API qbCamera qb_camera_perspective(float fov, float aspect, float near, float far, vec3s eye, vec3s center, vec3s up);
QB_API void qb_camera_resize(qbCamera camera, uint32_t width, uint32_t height);
QB_API void qb_camera_destroy(qbCamera* camera);


QB_API vec3s qb_camera_screentoworld(qbCamera camera, vec2s screen);
QB_API vec2s qb_camera_worldtoscreen(qbCamera camera, vec3s world);

QB_API void qb_light_enable(qbId id, qbLightType light_type);
QB_API void qb_light_disable(qbId id, qbLightType light_type);
QB_API bool qb_light_isenabled(qbId id, qbLightType light_type);

QB_API void qb_light_directional(qbId id, vec3s rgb, vec3s dir, float brightness);
QB_API void qb_light_point(qbId id, vec3s rgb, vec3s pos, float linear, float quadratic, float radius);

QB_API size_t qb_light_getmax(qbLightType light_type);

QB_API qbResult qb_render(qbRenderEvent event,
                          void(*on_render)(struct qbRenderEvent_*, qbVar),
                          void(*on_postrender)(struct qbRenderEvent_*, qbVar),
                          qbVar on_render_arg, qbVar on_postrender_arg);
QB_API qbEvent qb_render_event();

QB_API qbRenderer qb_renderer();
QB_API qbResult qb_render_swapbuffers();
QB_API qbResult qb_render_makecurrent();
QB_API qbResult qb_render_makenull();

typedef struct qbTransform_ {
  vec3s offset;
  vec3s position;
  mat4s orientation;
  vec3s scale;
} qbTransform_, *qbTransform;

QB_API void qb_modelgroup_create(qbModelGroup* modelgroup);
QB_API void qb_modelgroup_destroy(qbModelGroup* modelgroup);

// Uploads model and material to GPU.
QB_API void qb_modelgroup_upload(qbModelGroup modelgroup, struct qbModel_* model,
                                 struct qbMaterial_* material);

QB_API qbRenderGroup qb_modelgroup_rendergroup(qbModelGroup modelgroup);


// Component type: tag
QB_API qbComponent qb_renderable();

// Component type: qbModelGroup
QB_API qbComponent qb_modelgroup();

// Component type: qbMaterial
QB_API qbComponent qb_material();

// Component type: qbTransform_
QB_API qbComponent qb_transform();

// Component type: qbCollider_
QB_API qbComponent qb_collider();

#endif  // CUBEZ_RENDER__H
