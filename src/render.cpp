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

#include <cubez/render.h>
#include <cubez/input.h>
#include "shader.h"
#include "render_internal.h"
#include "gui_internal.h"

#include <atomic>

#define GL3_PROTOTYPES 1

#include <GL/glew.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <vector>
#include "render_defs.h"
#include "sparse_map.h"
#include "sparse_set.h"

#include <cglm/struct.h>

typedef struct qbCameraInternal_ {
  qbCamera_ camera;
  qbFrameBuffer fbo;
} qbCameraInternal_, *qbCameraInternal;

typedef struct qbRenderable_ {
  struct qbModel_* model;
  struct qbRenderGroup_* render_group;
  
  bool is_dirty;
} qbRenderable_, *qbRenderable;

// Channels
qbEvent render_event;

// Systems
qbSystem render_system;

int window_width;
int window_height;

SDL_Window *win = nullptr;
SDL_GLContext context;

qbRenderer renderer_;
void(*destroy_renderer)(struct qbRenderer_* renderer);

qbCameraInternal active_camera;
std::vector<qbCameraInternal> cameras;

qbId light_id;
SparseSet directional_lights;
SparseSet point_lights;
SparseSet spot_lights;
SparseSet enabled;

qbComponent qb_renderable_ = 0;
qbComponent qb_material_ = 0;
qbComponent qb_transform_ = 0;

qbComponent qb_renderable() {
  return qb_renderable_;
}

qbComponent qb_material() {
  return qb_material_;
}

qbComponent qb_transform() {
  return qb_transform_;
}

bool check_for_gl_errors() {
  GLenum error = glGetError();
  if (error == GL_NO_ERROR) {
    return false;
  }
  while (error != GL_NO_ERROR) {
    const GLubyte* error_str = gluErrorString(error);
    std::cout << "Error(" << error << "): " << error_str << std::endl;
    error = glGetError();
  }
  return true;
}

qbResult qb_render_swapbuffers() {
  SDL_GL_SwapWindow(win);
  return QB_OK;
}

qbEvent qb_render_event() {
  return render_event;
}

qbResult qb_render(qbRenderEvent event) {
  qb_render_makecurrent();
  qb_event_sendsync(render_event, event);
  qb_render_swapbuffers();
  return QB_OK;
}

void initialize_context(const RenderSettings& settings) {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
    printf("SDL could not initialize! SDL Error: %s\n", SDL_GetError());
  }
  
  // Request an OpenGL 3.3 context
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

  SDL_GL_SetAttribute(
    SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

  win = SDL_CreateWindow(settings.title,
                         SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         settings.width, settings.height,
                         SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);

  context = SDL_GL_CreateContext(win);

  // Disable vsync.
  SDL_GL_SetSwapInterval(0);

  glewExperimental = GL_TRUE;
  GLenum glewError = glewInit();
  if (glewError != 0) {
    std::cout << "Failed to intialize Glew\n"
              << "Error code: " << glewError;
    exit(1);
  }

  std::cout << "Using OpenGL " << glGetString(GL_VERSION) << std::endl;

  // Setting glewExperimental can cause an "INVALID_ENUM" OpenGL error. Swallow
  // that error here and carry on.
  glGetError();
  SDL_GL_SwapWindow(win);
}

void renderer_initialize(const RenderSettings& settings) {
  renderer_ = settings.create_renderer(settings.width, settings.height, settings.opt_renderer_args);
  renderer_->set_gui_renderpass(qb_renderer(), gui_create_renderpass(settings.width, settings.height));
  destroy_renderer = settings.destroy_renderer;
}

void render_initialize(RenderSettings* settings) {
  std::cout << "Initializing rendering context\n";
  initialize_context(*settings);  
  window_width = settings->width;
  window_height = settings->height;
  light_id = 0;
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, qbRenderable);
    qb_component_create(&qb_renderable_, attr);
    qb_componentattr_destroy(&attr);
  }
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, qbTransform_);
    qb_component_create(&qb_transform_, attr);
    qb_componentattr_destroy(&attr);
  }
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, qbMaterial);
    qb_component_create(&qb_material_, attr);
    qb_componentattr_destroy(&attr);
  }
  {
    qbEventAttr attr;
    qb_eventattr_create(&attr);
    qb_eventattr_setmessagetype(attr, qbRenderEvent_);
    qb_event_create(&render_event, attr);
    qb_eventattr_destroy(&attr);
  }
  gui_initialize();
  renderer_initialize(*settings);
}

void render_shutdown() {
  SDL_GL_DeleteContext(context);
  SDL_DestroyWindow(win);
  destroy_renderer(renderer_);
  renderer_ = nullptr;
}

uint32_t qb_window_width() {
  return window_width;
}

uint32_t qb_window_height() {
  return window_height;
}

void qb_window_resize(uint32_t width, uint32_t height) {
  window_width = width;
  window_height = height;
}

void qb_camera_create(qbCamera* camera_ref, qbCameraAttr attr) {
  qbCameraInternal cam = new qbCameraInternal_;
  *camera_ref = (qbCamera)cam;
  qbCamera_* camera = (qbCamera_*)*camera_ref;
  memset(camera, 0, sizeof(qbCamera_));

  camera->width = attr->width;
  camera->height = attr->height;
  camera->ratio = (float)attr->width / (float)attr->height;
  camera->fov = attr->fov;
  camera->znear = attr->znear;
  camera->zfar = attr->zfar;
  
  camera->view_mat = glms_lookat(
    vec3s{ 0.0f, 0.0f, 0.0f },
    vec3s{ -1.0f, 0.0f, 0.0f },
    vec3s{ 0.0f, 0.0f, 1.0f }
  );

  camera->projection_mat = glms_perspective(camera->fov, camera->ratio, camera->znear, camera->zfar);
  camera->rotation_mat= attr->rotation_mat;
  camera->origin = attr->origin;

  vec3s forward = glms_mat4_mulv3(camera->rotation_mat, { 1.0f, 0.0f, 0.0f }, 1.0f);
  vec3s up = glms_mat4_mulv3(camera->rotation_mat, { 0.0f, 0.0f, 1.0f }, 1.0f);
  camera->view_mat = glms_look(camera->origin, forward, up);
  camera->forward = forward;
  camera->up = up;

  auto r = qb_renderer();
  cam->fbo = r->camera_framebuffer_create(r, camera->width, camera->height);;
  cameras.push_back(cam);
}

void qb_camera_destroy(qbCamera* camera) {
  delete *camera;
  *camera = nullptr;
}

void qb_camera_activate(qbCamera camera) {
  active_camera = (qbCameraInternal)camera;
  qbRenderPipeline r = qb_renderer()->render_pipeline;
  
  qbRenderPass* passes;
  size_t count = qb_renderpipeline_passes(r, &passes);
  for (size_t i = 0; i < count; ++i) {
    qb_renderpass_setframe(passes[i], ((qbCameraInternal)camera)->fbo);
  }
}

void qb_camera_deactivate(qbCamera camera) {
  active_camera = nullptr;
}

qbCamera qb_camera_active() {
  return (qbCamera)active_camera;
}

void qb_camera_resize(qbCamera camera_ref, uint32_t width, uint32_t height) {
  qbCamera_* camera = (qbCamera_*)camera_ref;

  if (camera->width == width && camera->height == height) {
    return;
  }

  camera->width = width;
  camera->height = height;
  camera->ratio = (float)width / (float)height;
  camera->projection_mat = glms_perspective(camera->fov, camera->ratio, camera->znear, camera->zfar);
  
  qbCameraInternal internal = (qbCameraInternal)camera_ref;
  qb_framebuffer_resize(internal->fbo, width, height);
}

void qb_camera_fov(qbCamera camera_ref, float fov) {
  qbCamera_* camera = (qbCamera_*)camera_ref;
  camera->fov = fov;
  camera->projection_mat = glms_perspective(camera->fov, camera->ratio, camera->znear, camera->zfar);
}

void qb_camera_clip(qbCamera camera_ref, float znear, float zfar) {
  qbCamera_* camera = (qbCamera_*)camera_ref;
  if (camera->znear == znear && camera->zfar == zfar) {
    return;
  }

  camera->zfar = zfar;
  camera->znear = znear;
  camera->projection_mat = glms_perspective(camera->fov, camera->ratio, camera->znear, camera->zfar);
}

void qb_camera_rotation(qbCamera camera_ref, mat4s rotation) {
  qbCamera_* camera = (qbCamera_*)camera_ref;
  camera->rotation_mat = rotation;

  static vec4s forward = { 1.0f, 0.0f, 0.0f, 1.0f };
  static vec4s up = { 0.0f, 0.0f, 1.0f, 1.0f };
  camera->forward = glms_vec3(glms_mat4_mulv(camera->rotation_mat, forward));
  camera->up = glms_vec3(glms_mat4_mulv(camera->rotation_mat, up));
  camera->view_mat = glms_look(camera->origin, camera->forward, camera->up);
}

void qb_camera_origin(qbCamera camera_ref, vec3s  origin) {
  qbCamera_* camera = (qbCamera_*)camera_ref;
  camera->origin = origin;

  static vec4s forward = { 1.0f, 0.0f, 0.0f, 1.0f };
  static vec4s up = { 0.0f, 0.0f, 1.0f, 1.0f };
  camera->forward = glms_vec3(glms_mat4_mulv(camera->rotation_mat, forward));
  camera->up = glms_vec3(glms_mat4_mulv(camera->rotation_mat, up));
  camera->view_mat = glms_look(camera->origin, camera->forward, camera->up);
}

vec3s qb_camera_screentoworld(qbCamera camera, vec2s screen) {
  // NORMALISED DEVICE SPACE
  float x = 2.0f * screen.x / (float)qb_window_width() - 1.0f;
  float y = 2.0f * screen.y / (float)qb_window_height() - 1.0f;
  
  // HOMOGENEOUS SPACE
  vec4s screenPos = { x, -y, 1.0f, 1.0f };

  // Projection/Eye Space
  mat4s project_view = glms_mat4_mul(camera->projection_mat, camera->view_mat);
  mat4s view_projection_inverse = glms_mat4_inv(project_view);

  vec4s world_coord = glms_mat4_mulv(view_projection_inverse, screenPos);
  world_coord.w = 1.0f / world_coord.w;
  world_coord.x *= world_coord.w;
  world_coord.y *= world_coord.w;
  world_coord.z *= world_coord.w;

  return glms_vec3(glms_vec4_normalize(world_coord));
}

vec2s qb_camera_worldtoscreen(qbCamera camera, vec3s world) {
  mat4s project_view = glms_mat4_mul(camera->projection_mat, camera->view_mat);
  return glms_vec2(glms_mat4_mulv3(project_view, world, 1.0f));
}

QB_API qbFrameBuffer qb_camera_fbo(qbCamera camera) {
  return ((qbCameraInternal)camera)->fbo;
}

qbResult qb_render_makecurrent() {
  int ret = SDL_GL_MakeCurrent(win, context);
  if (ret < 0) {
    std::cout << "SDL_GL_MakeCurrent failed: " << SDL_GetError() << std::endl;
  }
  return QB_OK;
}

qbResult qb_render_makenull() {
  int ret = SDL_GL_MakeCurrent(win, nullptr);
  if (ret < 0) {
    std::cout << "SDL_GL_MakeCurrent failed: " << SDL_GetError() << std::endl;
  }
  return QB_OK;
}

qbRenderer qb_renderer() {
  return renderer_;
}

void qb_renderable_create(qbRenderable* renderable, struct qbModel_* model) {
  qbRenderable ret = new qbRenderable_();
  ret->model = model;

  qbRenderGroup group;
  {
    qbRenderGroupAttr_ attr = {};
    qb_rendergroup_create(&group, &attr);
  }

  for (size_t i = 0; i < model->mesh_count; ++i) {
    qbMeshBuffer buffer;
    qb_mesh_tobuffer(model->meshes + i, &buffer);
    qb_rendergroup_append(group, buffer);
  }
  qbRenderer renderer = qb_renderer();
  renderer->rendergroup_oncreate(renderer, group);
  renderer->rendergroup_add(renderer, group);

  ret->render_group = group;
  ret->is_dirty = true;
  *renderable = ret;
}

void qb_renderable_destroy(qbRenderable* renderable) {
  renderer_->rendergroup_ondestroy(renderer_, (*renderable)->render_group);

  if ((*renderable)->model) {
    qb_model_destroy(&(*renderable)->model);
  }

  if ((*renderable)->render_group) {
    qbRenderGroup group = (*renderable)->render_group;
    qbRenderer renderer = qb_renderer();
    renderer->rendergroup_remove(renderer, (*renderable)->render_group);
    
    qbMeshBuffer* buffers;
    size_t mesh_count = qb_rendergroup_meshes(group, &buffers);
    for (size_t i = 0; i < mesh_count; ++i) {
      qb_meshbuffer_destroy(buffers + i);
    }
    qb_rendergroup_destroy(&(*renderable)->render_group);
  }
}

void qb_renderable_free(qbRenderable renderable) {
  if (renderable->render_group) {
    qbRenderGroup group = renderable->render_group;
    qbRenderer renderer = qb_renderer();
    renderer->rendergroup_remove(renderer, group);

    qbMeshBuffer* buffers;
    size_t mesh_count = qb_rendergroup_meshes(group, &buffers);
    for (size_t i = 0; i < mesh_count; ++i) {
      qb_meshbuffer_destroy(buffers + i);
    }
    qb_rendergroup_destroy(&renderable->render_group);
  }
}

void qb_renderable_update(qbRenderable renderable, struct qbModel_* model) {
  assert(false && "unimplemented");
}

void qb_renderable_upload(qbRenderable renderable, struct qbMaterial_* material) {
  qbRenderer renderer = qb_renderer();

  if (!renderable->render_group) {
    qbRenderGroupAttr_ attr;
    qb_rendergroup_create(&renderable->render_group, &attr);

    for (size_t i = 0; i < renderable->model->mesh_count; ++i) {
      qbMeshBuffer buffer;
      qb_mesh_tobuffer(renderable->model->meshes + i, &buffer);
      qb_rendergroup_append(renderable->render_group, buffer);
    }
    
    renderer->rendergroup_oncreate(renderer, renderable->render_group);
    renderer->rendergroup_add(renderer, renderable->render_group);
  }

  if (renderable->is_dirty) {
    renderer->rendergroup_attach_material(renderer, renderable->render_group, material);
    renderer->rendergroup_attach_textures(renderer, renderable->render_group,
                                          material->image_count, material->image_units,
                                          material->images);
    renderer->rendergroup_attach_uniforms(renderer, renderable->render_group,
                                          material->uniform_count, material->uniform_bindings,
                                          material->uniforms);
    renderable->is_dirty = false;
  }
}

struct qbModel_* qb_renderable_model(qbRenderable renderable) {
  return renderable->model;
}

qbRenderGroup qb_renderable_rendergroup(qbRenderable renderable) {
  return renderable->render_group;
}

void qb_light_enable(qbId id, qbLightType type) {
  auto r = qb_renderer();
  r->light_enable(r, id, type);
}

void qb_light_disable(qbId id, qbLightType type) {
  auto r = qb_renderer();
  r->light_disable(r, id, type);
}

bool qb_light_isenabled(qbId id, qbLightType type) {
  auto r = qb_renderer();
  return r->light_isenabled(r, id, type);
}

void qb_light_directional(qbId id, vec3s  rgb, vec3s  dir, float brightness) {
  auto r = qb_renderer();
  r->light_directional(r, id, rgb, dir, brightness);
}

void qb_light_point(qbId id, vec3s  rgb, vec3s  pos, float brightness, float range) {
  auto r = qb_renderer();
  r->light_point(r, id, rgb, pos, brightness, range);
}

void qb_light_spotlight(qbId id, vec3s  rgb, vec3s  pos, vec3s  dir, float brightness, float range, float angle_deg) {
  auto r = qb_renderer();
  r->light_spot(r, id, rgb, pos, dir, brightness, range, angle_deg);
}

size_t qb_light_max(qbLightType light_type) {
  auto r = qb_renderer();
  return r->light_max(r, light_type);
}
