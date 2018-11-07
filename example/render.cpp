#include "physics.h"
#include "player.h"
#include "render.h"
#include "shader.h"
#include "input.h"
#include "planet.h"

#include "gui.h"

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
  glm::vec3 origin;

  float yaw = 0.0f;
  float pitch = 0.0f;

  const glm::vec4 front = { 1.0f, 0.0f, 0.0f, 1.0f };
  const glm::vec4 up = { 0.0f, 0.0f, 1.0f, 1.0f };
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
SDL_GLContext context;

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

void render_event_handler(qbInstance* insts, qbFrame* f) {
  RenderEvent* e = (RenderEvent*)f->event;

  qbRenderable* renderable;
  qb_instance_getconst(insts[0], &renderable);
  
  physics::Transform* transform;
  qb_instance_getconst(insts[1], &transform);
  glm::vec3 v = transform->v * (float)e->alpha;

  glm::mat4 mvp;
  glm::mat4 m = glm::translate(glm::mat4(1.0f), transform->p + v);

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
  planet::RenderPlanetPopup();
  qb_render_makecurrent();

  // Initial rotation matrix.
  camera.rotation_mat = glm::mat4(1.0f);

  glm::vec3 look_from = camera.from + camera.origin;
  glm::vec3 look_to = camera.origin;
  glm::vec3 direction = glm::normalize(look_from - look_to);
  glm::vec3 up = camera.up;
  glm::vec3 right = glm::normalize(glm::cross(up, direction));
  up = glm::cross(direction, right);

  camera.view_mat = glm::lookAt(look_from, look_to, up);

  glViewport(0, 0, window_width(), window_height());
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);
  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);

  glClearColor(0.1, 0.1, 0.1, 0.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  qb_event_sendsync(render_event, event);
  gui::Render();
  SDL_GL_SwapWindow(win);
  qb_render_makenull();
}

qbRenderable create(qbMesh mesh, qbMaterial material) {
  return new qbRenderable_{mesh, material};
}

void initialize_context(const Settings& settings) {

  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
  
  // Request an OpenGL 3.3 context
  const char* glsl_version = "#version 130";
  SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

  SDL_GL_SetAttribute(
      SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);

  win = SDL_CreateWindow(settings.title.c_str(),
                         SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         settings.width, settings.height,
                         SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI);

  context = SDL_GL_CreateContext(win);

  // Enable vsync.
  SDL_GL_SetSwapInterval(-1);

  glewExperimental = GL_TRUE;
  GLenum glewError = glewInit();
  if (glewError != 0) {
    std::cout << "Failed to intialize Glew\n"
              << "Error code: " << glewError;
    exit(1);
  }

  gui::Initialize(win);

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
  camera.origin = { 0.0f, 0.0f, 0.0f };

  // There's a closed form solution that might be faster, but this is performed
  // only once here.
  camera.inv_projection_mat = glm::inverse(camera.projection_mat);

  camera.from = {0.0f, 0.0f, 750.0f};
  camera.to = {0.0f, 0.0f, 0.0f};
  camera.rotation_mat = glm::mat4(1.0f);
  camera.yaw = 0.0f;
  camera.pitch = 0.0f;


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
  gui::Shutdown();
  SDL_GL_DeleteContext(context);
  SDL_DestroyWindow(win);
}

int window_height() {
  return height;
}

int window_width() {
  return width;
}

qbResult qb_camera_setorigin(glm::vec3 new_origin) {
  camera.origin = new_origin;
  return QB_OK;
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

glm::mat4 qb_camera_getprojection() {
  return camera.projection_mat;
}

glm::mat4 qb_camera_getinvprojection() {
  return camera.inv_projection_mat;
}

qbResult qb_camera_screentoworld(glm::vec2 screen, glm::vec3* world) {
  // NORMALISED DEVICE SPACE
  double x = 2.0 * screen.x / window_width() - 1;
  double y = 2.0 * screen.y / window_height() - 1;
  
  // HOMOGENEOUS SPACE
  glm::vec4 screenPos = glm::vec4(x, -y, 1.0f, 1.0f);

  // Projection/Eye Space
  glm::mat4 project_view = camera.projection_mat * camera.view_mat;
  glm::mat4 view_projection_inverse = glm::inverse(project_view);

  glm::vec4 world_coord = view_projection_inverse * screenPos;
  world_coord.w = 1.0f / world_coord.w;
  world_coord.x *= world_coord.w;
  world_coord.y *= world_coord.w;
  world_coord.z *= world_coord.w;
  *world = world_coord;
  *world = glm::normalize(*world);
  return QB_OK;
}

glm::vec3 qb_camera_getorigin() {
  return camera.origin;
}

float qb_camera_getyaw() {
  return camera.yaw;
}

float qb_camera_getpitch() {
  return camera.pitch;
}

float qb_camera_getznear() {
  return camera.znear;
}

float qb_camera_getzfar() {
  return camera.zfar;
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

}  // namespace render
