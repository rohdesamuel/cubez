#include "physics.h"
#include "player.h"
#include "render.h"
#include "shader.h"

#include "constants.h"

#include <atomic>
#include <glm/gtc/matrix_transform.hpp>

#define GL3_PROTOTYPES 1

#include <GL/glew.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

namespace render {

struct qbRenderable_ {
  qbMesh mesh;
  qbMaterial material;
};

struct Camera {
  float ratio;
  float fov;
  float znear;
  float zfar;

  glm::mat4 view_mat;
  glm::mat4 projection_mat;
  glm::mat4 inv_projection_mat;
  glm::mat4 rotation_mat;
  glm::vec3 from;
  glm::vec3 to;

  float yaw = 0.0f;
  float pitch = 0.0f;

  const glm::vec4 up_vector = {0.0f, 0.0f, 1.0f, 1.0f};
} camera;

// Collections
qbComponent renderables;

// Channels
qbEvent render_event;

// Systems
qbSystem render_system;

int width;
int height;

SDL_Window *win = nullptr;
SDL_GLContext *context = nullptr;

qbComponent component() {
  return renderables;
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

void render_event_handler(qbInstance* insts, qbFrame*) {
  qbRenderable* renderable;
  qb_instance_getconst(insts[0], &renderable);
  
  physics::Transform* transform;
  qb_instance_getconst(insts[1], &transform);

  glm::mat4 mvp;
  glm::mat4 m = glm::translate(glm::mat4(1.0f), transform->p);

  qb_material_use((*renderable)->material);

  mvp = camera.projection_mat * camera.view_mat * m;
  qbShader shader = qb_material_getshader((*renderable)->material);
  qb_shader_setmat4(shader, "uMvp", mvp);

  qb_mesh_draw((*renderable)->mesh, (*renderable)->material);
  
  if (check_for_gl_errors()) {
    std::cout << "Renderable entity: " << qb_instance_getentity(insts[0]) << "\n";
  }
}

void present(RenderEvent* event) {
  // Initial rotation matrix.
  camera.rotation_mat = glm::mat4(1.0f);

  // Rotatae yaw.
  camera.rotation_mat = glm::rotate(
      camera.rotation_mat,
      glm::radians(camera.yaw),
      glm::vec3{0.0f, 0.0f, 1.0f});

  // Rotate pitch.
  camera.rotation_mat = glm::rotate(
      camera.rotation_mat,
      glm::radians(camera.pitch),
      glm::vec3{0.0f, 1.0f, 0.0f});

  glm::vec3 look_from = {0.0f, 0.0f, 0.0f};
  glm::vec3 look_to = {1.0f, 0.0f, 0.0f};
  
  look_from = glm::vec3(camera.rotation_mat * glm::vec4(look_from, 1.0f));
  look_to = glm::vec3(camera.rotation_mat * glm::vec4(look_to, 1.0f));

  look_from += camera.from;
  look_to += camera.from;

  camera.view_mat = glm::lookAt(
      look_from, look_to,
      glm::vec3(camera.rotation_mat * camera.up_vector));

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  qb_event_sendsync(render_event, event);
  SDL_GL_SwapWindow(win);
}

qbRenderable create(qbMesh mesh, qbMaterial material) {
  return new qbRenderable_{mesh, material};
}

void initialize_context(const Settings& settings) {
  int posX = 100, posY = 100;
  
  SDL_Init(SDL_INIT_VIDEO);
  
  // Request an OpenGL 3.3 context
  SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

  SDL_GL_SetAttribute(
      SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);

  win = SDL_CreateWindow(settings.title.c_str(), posX, posY,
                         settings.width, settings.height,
                         SDL_WINDOW_OPENGL);

  SDL_GL_CreateContext(win);

  // Enable vsync.
  SDL_GL_SetSwapInterval(-1);

  glewExperimental = GL_TRUE;
  GLenum glewError = glewInit();
  if (glewError != 0) {
    std::cout << "Failed to intialize Glew\n"
              << "Error code: " << glewError;
    exit(1);
  }

  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);
  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);

  glClearColor(0.1, 0.1, 0.1, 0.0);

  std::cout << "Using OpenGL " << glGetString(GL_VERSION) << std::endl;

  // Setting glewExperimental can cause an "INVALID_ENUM" OpenGL error. Swallow
  // that error here and carry on.
  glGetError();

  SDL_GL_SwapWindow(win);
}

void initialize(const Settings& settings) {
  std::cout << "Initializing rendering context\n";
  initialize_context(settings);

  height = settings.height;
  width = settings.width;
  camera.ratio = (float) width / (float) height;
  camera.fov = settings.fov;
  camera.znear = settings.znear;
  camera.zfar = settings.zfar;
  camera.projection_mat = glm::perspective(
      glm::radians(camera.fov),
      camera.ratio,
      camera.znear,
      camera.zfar);

  // There's a closed form solution that might be faster, but this is performed
  // only once here.
  camera.inv_projection_mat = glm::inverse(camera.projection_mat);

  camera.from = {0.0f, 0.0f, 750.0f};
  camera.to = {0.0f, 0.0f, 0.0f};
  camera.rotation_mat = glm::mat4(1.0f);
  camera.yaw = 0.0f;
  camera.pitch = -90.0f;

  // Rotate yaw.
  camera.rotation_mat = glm::rotate(
      camera.rotation_mat,
      glm::radians(camera.yaw),
      glm::vec3{0.0f, 0.0f, 1.0f});

  // Rotate pitch.
  camera.rotation_mat = glm::rotate(
      camera.rotation_mat,
      glm::radians(camera.pitch),
      glm::vec3{1.0f, 0.0f, 0.0f});

  camera.view_mat = glm::lookAt(
      camera.from, camera.to,
      glm::vec3(camera.rotation_mat * camera.up_vector));

  // Initialize collections.
  {
    std::cout << "Intializing renderables collection\n";
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, qbRenderable);

    qb_component_create(&renderables, attr);
    qb_componentattr_destroy(&attr);
  }

  // Initialize systems.
  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_settrigger(attr, qbTrigger::QB_TRIGGER_EVENT);
    qb_systemattr_setpriority(attr, QB_MIN_PRIORITY);
    qb_systemattr_addconst(attr, renderables);
    qb_systemattr_addconst(attr, physics::component());
    qb_systemattr_setjoin(attr, qbComponentJoin::QB_JOIN_INNER);
    qb_systemattr_setfunction(attr, render_event_handler);

    qb_system_create(&render_system, attr);
    qb_systemattr_destroy(&attr);
  }

  // Initialize events.
  {
    qbEventAttr attr;
    qb_eventattr_create(&attr);
    qb_eventattr_setmessagesize(attr, sizeof(RenderEvent));

    qb_event_create(&render_event, attr);
    qb_event_subscribe(render_event, render_system);
    qb_eventattr_destroy(&attr);
  }

  std::cout << "Finished initializing render\n";
}

void shutdown() {
  SDL_DestroyWindow(win);
}

int window_height() {
  return height;
}

int window_width() {
  return width;
}

qbResult qb_camera_setposition(glm::vec3 new_position) {
  glm::vec3 delta = new_position - camera.from;

  camera.from += delta;
  camera.to += delta;
  return QB_OK;
}

qbResult qb_camera_setyaw(float new_yaw) {
  camera.yaw = new_yaw;
  return QB_OK;
}

qbResult qb_camera_setpitch(float new_pitch) {
  camera.pitch = new_pitch;
  return QB_OK;
}

qbResult qb_camera_incposition(glm::vec3 delta) {
  camera.from += delta;
  camera.to += delta;
  return QB_OK;
}

qbResult qb_camera_incyaw(float delta) {
  camera.yaw += delta;
  return QB_OK;
}

qbResult qb_camera_incpitch(float delta) {
  camera.pitch += delta;
  return QB_OK;
}

glm::vec3 qb_camera_getposition() {
  return camera.from;
}

glm::mat4 qb_camera_getorientation() {
  return camera.rotation_mat;
}

float qb_camera_getyaw() {
  return camera.yaw;
}

float qb_camera_getpitch() {
  return camera.pitch;
}


}  // namespace render
