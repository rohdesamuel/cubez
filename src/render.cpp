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

#include <cubez/render.h>
#include <cubez/input.h>
#include "shader.h"
#include "render_internal.h"
#include "gui_internal.h"
#include <cubez/gui.h>

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

// Channels
qbEvent render_event;

// Systems
qbSystem render_system;

int window_width;
int window_height;

SDL_Window *win = nullptr;
SDL_GLContext context;

qbRenderer renderer_ = nullptr;
void(*destroy_renderer)(struct qbRenderer_* renderer) = nullptr;

typedef enum class qbCameraType {
  ORTHO,
  PERSPECTIVE
};

typedef struct qbCameraInternal_ {
  qbCamera_ camera;

  qbCameraType type;
} qbCameraInternal_, *qbCameraInternal;

std::vector<qbCameraInternal> cameras;

qbId light_id;
SparseSet directional_lights;
SparseSet point_lights;
SparseSet spot_lights;
SparseSet enabled;

qbComponent qb_renderable_ = 0;
qbComponent qb_modelgroup_ = 0;
qbComponent qb_material_ = 0;
qbComponent qb_transform_ = 0;
qbComponent qb_collider_ = 0;

qbComponent qb_renderable() {
  return qb_renderable_;
}

qbComponent qb_modelgroup() {
  return qb_modelgroup_;
}

qbComponent qb_material() {
  return qb_material_;
}

qbComponent qb_transform() {
  return qb_transform_;
}

qbComponent qb_collider() {
  return qb_collider_;
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

qbResult qb_render(qbRenderEvent event,
                   void(*on_render)(struct qbRenderEvent_*, qbVar),
                   void(*on_postrender)(struct qbRenderEvent_*, qbVar),
                   qbVar on_render_arg, qbVar on_postrender_arg) {
  if (render_event) {
    qb_render_makecurrent();
    if (on_render) {
      on_render(event, on_render_arg);
    }
    qb_renderer()->render(qb_renderer(), event);
    qb_event_sendsync(render_event, event);

    if (on_postrender) {
      on_postrender(event, on_postrender_arg);
    }

    qb_render_swapbuffers();
  }
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
  if (!context) {
    std::cout << "Could not create OpenGL context: " << SDL_GetError();
  }

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
  if (!settings.opt_renderer_args->opt_gui_renderpass) {
    settings.opt_renderer_args->opt_gui_renderpass = gui_create_renderpass(settings.width, settings.height);
  }

  if (settings.create_renderer) {
    renderer_ = settings.create_renderer(settings.width, settings.height, settings.opt_renderer_args);
  } else {
    renderer_ = nullptr;
  }

  if (settings.destroy_renderer) {
    destroy_renderer = settings.destroy_renderer;
  } else {
    destroy_renderer = [](qbRenderer) {};
  }
}

void render_initialize(RenderSettings* settings) {
  initialize_context(*settings);  
  window_width = settings->width;
  window_height = settings->height;
  light_id = 0;
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_component_create(&qb_renderable_, "qbRenderable", attr);
    qb_componentattr_destroy(&attr);
  }
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, qbModelGroup);
    qb_component_create(&qb_modelgroup_, "qbModelgroup", attr);
    qb_componentattr_destroy(&attr);
  }
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, qbTransform_);
    qb_component_create(&qb_transform_, "qbTransform", attr);
    qb_componentattr_destroy(&attr);
  }
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, qbMaterial);
    qb_component_create(&qb_material_, "qbMaterial", attr);
    qb_componentattr_destroy(&attr);
  }
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, qbCollider_);
    qb_component_create(&qb_collider_, "qbCollider", attr);
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
  if (destroy_renderer) {
    destroy_renderer(renderer_);
  }
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

  for (auto c : cameras) {
    qb_camera_resize((qbCamera)c, width, height);
  }
  if (renderer_) {
    renderer_->resize(renderer_, width, height);
  }
  qb_gui_resize(width, height);
}

void qb_window_setfullscreen(qbFullscreenType type) {
  uint32_t flags = 0;
  if (type == QB_WINDOW_FULLSCREEN) {
    flags = SDL_WINDOW_FULLSCREEN;
  } else if (type == QB_WINDOW_FULLSCREEN_DESKTOP) {
    flags = SDL_WINDOW_FULLSCREEN_DESKTOP;
  }

  SDL_SetWindowFullscreen(win, flags);
}

qbFullscreenType qb_window_fullscreen() {
  uint32_t flags = SDL_GetWindowFlags(win);

  if (flags & SDL_WINDOW_FULLSCREEN) {
    return QB_WINDOW_FULLSCREEN;
  }

  if (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) {
    return QB_WINDOW_FULLSCREEN_DESKTOP;
  }

  return QB_WINDOWED;
}

void qb_window_setbordered(int bordered) {
  SDL_SetWindowBordered(win, (SDL_bool)bordered);
}

int qb_window_bordered() {
  return SDL_GetWindowFlags(win) & SDL_WINDOW_BORDERLESS ? 0 : 1;
}

qbCamera qb_camera_ortho(float left, float right, float bottom, float top, vec2s eye) {
  float width = right - left;
  float height = bottom - top;

  mat4s projection_mat = glms_ortho(left, right, bottom, top, -1.f, 1.f);
  mat4s view_mat = glms_mat4_inv(glms_translate(glms_mat4_identity(), { eye.x, eye.y, 0.f }));
  qbCameraInternal ret = new qbCameraInternal_{
    .camera = {
      .aspect = width / height,
      .near = -1.f,
      .far = 1.f,
      .fov = 1.f,

      .eye = {width / 2, height / 2, 0.f},
      .view_mat = view_mat,
      .projection_mat = projection_mat
    },
    .type = qbCameraType::ORTHO
  };

  cameras.push_back(ret);
  return (qbCamera)ret;
}

qbCamera qb_camera_perspective(
  float fov, float aspect, float near, float far,
  vec3s eye, vec3s center, vec3s up) {

  mat4s view = glms_lookat(eye, center, up);
  mat4s proj = glms_perspective(fov, aspect, near, far);

  qbCameraInternal ret = new qbCameraInternal_{
    .camera = {
      .aspect = aspect,
      .near = near,
      .far = far,
      .fov = fov,

      .eye = eye,
      .view_mat = view,
      .projection_mat = proj
    },
    .type = qbCameraType::PERSPECTIVE
  };

  cameras.push_back(ret);
  return (qbCamera)ret;
}

void qb_camera_destroy(qbCamera* camera) {
  if (!camera || !*camera) {
    return;
  }

  cameras.erase(std::find(cameras.begin(), cameras.end(), (qbCameraInternal)(*camera)));

  delete *camera;
  *camera = nullptr;
}

void qb_camera_resize(qbCamera camera_ref, uint32_t width, uint32_t height) {
  if (!camera_ref) {
    return;
  }

  qbCamera camera = &((qbCameraInternal)camera_ref)->camera;

  float aspect = (float)width / (float)height;
  camera->aspect = aspect;
  if (((qbCameraInternal)camera_ref)->type == qbCameraType::ORTHO) {
    float fov = camera->fov;
    camera->projection_mat = glms_ortho(-aspect * fov, aspect * fov, -fov, fov, -1.f, 1.f);

  } else {
    camera->projection_mat = glms_perspective(camera->fov, camera->aspect, camera->near, camera->far);
  }  
}

vec3s qb_camera_screentoworld(qbCamera camera, vec2s screen) {
  if (!camera) {
    return{};
  }

  // NORMALISED DEVICE SPACE
  float x = 2.0f * screen.x / (float)qb_window_width() - 1.0f;
  float y = -2.0f * screen.y / (float)qb_window_height() + 1.0f;

  // HOMOGENEOUS SPACE
  vec4s near_plane = vec4s{ x, y, -1, 1.0 };
  vec4s far_plane = vec4s{ x, y, 1, 1.0 };

  // Projection/Eye Space
  mat4s project_view = glms_mat4_mul(camera->projection_mat, camera->view_mat);
  mat4s view_projection_inverse = glms_mat4_inv(project_view);

  vec4s near = glms_mat4_mulv(view_projection_inverse, near_plane);
  vec4s far = glms_mat4_mulv(view_projection_inverse, far_plane);
  near = glms_vec4_divs(near, near.w);
  far = glms_vec4_divs(far, far.w);

  return glms_vec3_normalize(glms_vec3(glms_vec4_sub(far, near)));
}

vec2s qb_camera_worldtoscreen(qbCamera camera, vec3s world) {
  if (!camera) {
    return{};
  }

  mat4s project_view = glms_mat4_mul(camera->projection_mat, camera->view_mat);
  return glms_vec2(glms_mat4_mulv3(project_view, world, 1.0f));
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

void qb_modelgroup_create(qbModelGroup* modelgroup) {
  qbRenderer r = qb_renderer();
  r->modelgroup_create(r, modelgroup);
}

void qb_modelgroup_destroy(qbModelGroup* modelgroup) {
  qbRenderer r = qb_renderer();
  r->modelgroup_destroy(r, modelgroup);
}

void qb_modelgroup_upload(qbModelGroup modelgroup,
                          struct qbModel_* model,
                          struct qbMaterial_* material) {
  qbRenderer r = qb_renderer();
  r->modelgroup_upload(r, modelgroup, model, material);
}

qbRenderGroup qb_modelgroup_rendergroup(qbModelGroup modelgroup) {
  qbRenderer r = qb_renderer();
  return r->modelgroup_rendergroup(r, modelgroup);
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

void qb_light_point(qbId id, vec3s  rgb, vec3s  pos, float linear, float quadratic, float range) {
  auto r = qb_renderer();
  r->light_point(r, id, rgb, pos, linear, quadratic, range);
}

void qb_light_spotlight(qbId id, vec3s  rgb, vec3s  pos, vec3s  dir, float brightness, float range, float angle_deg) {
  auto r = qb_renderer();
  r->light_spot(r, id, rgb, pos, dir, brightness, range, angle_deg);
}

size_t qb_light_getmax(qbLightType light_type) {
  auto r = qb_renderer();
  return r->light_max(r, light_type);
}