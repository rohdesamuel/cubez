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
  Material* material;
  Mesh* mesh;
};

struct Camera {
  float ratio;
  float fov;
  float znear;
  float zfar;

  glm::mat4 view_mat;
  glm::mat4 projection_mat;
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

void render_event_handler(qbElement* elements, qbCollectionInterface*, qbFrame*) {
  qbRenderable renderable;
  qb_element_read(elements[0], &renderable);

  Material* material = renderable->material;
  Mesh* mesh = renderable->mesh;

  physics::Transform transform;
  qb_element_read(elements[1], &transform);


  glBindVertexArray(mesh->vao);
  glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);

  ShaderProgram shaders(material->shader_id);
  shaders.use();

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, material->texture_id);
  glUniform1i(glGetUniformLocation(shaders.id(), "uTexture"), 0);

  glm::mat4 mvp;
  glm::mat4 m = glm::translate(glm::mat4(1.0f), transform.p);

  mvp = camera.projection_mat * camera.view_mat * m;
  glUniformMatrix4fv(glGetUniformLocation(
        shaders.id(), "uMvp"), 1, GL_FALSE, (GLfloat*)&mvp);
  glDrawArrays(GL_TRIANGLES, 0, mesh->count);
  
  if (check_for_gl_errors()) {
    std::cout << "Renderable id: " << qb_element_getid(elements[0]) << "\n";
  }
}

void present(RenderEvent* event) {
  camera.yaw += 1.0f;
  camera.pitch = 90.0f;

  camera.from = {0.0f, 0.0f, 0.0f};
  camera.to = {1.0f, 0.0f, 0.0f};
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
  
  camera.from = glm::vec3(camera.rotation_mat * glm::vec4(camera.from, 1.0f));
  camera.to = glm::vec3(camera.rotation_mat * glm::vec4(camera.to, 1.0f));

  camera.from += glm::vec3{0.0f, 0.0f, 100.0f};
  camera.to += glm::vec3{0.0f, 0.0f, 100.0f};

  camera.view_mat = glm::lookAt(
      camera.from, camera.to,
      glm::vec3(camera.rotation_mat * camera.up_vector));

  qb_event_send(render_event, event);
  qb_event_flush(render_event);
  SDL_GL_SwapWindow(win);
}

qbRenderable create(const Material& mat, const Mesh& mesh) {
  return new qbRenderable_{new Material(mat), new Mesh(mesh)};
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

  win = SDL_CreateWindow(settings.title.c_str(), posX, posY,
                         settings.width, settings.height,
                         SDL_WINDOW_OPENGL);

  SDL_GL_CreateContext(win);

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
  camera.from = {0.0f, 0.0f, 750.0f};
  camera.to = {0.0f, 0.0f, 0.0f};
  camera.rotation_mat = glm::mat4(1.0f);
  camera.yaw = 0.0f;
  camera.pitch = -90.0f;

  // Rotatae yaw.
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
    qb_systemattr_settrigger(attr, qbTrigger::EVENT);
    qb_systemattr_setpriority(attr, QB_MIN_PRIORITY);
    qb_systemattr_addsource(attr, renderables);
    qb_systemattr_addsource(attr, physics::component());
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

uint32_t load_texture(const std::string& file) {
  uint32_t texture;

  // Load the image from the file into SDL's surface representation
  SDL_Surface* surf = SDL_LoadBMP(file.c_str());

  if (!surf) {
    std::cout << "Could not load texture " << file << std::endl;
  }
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surf->w, surf->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surf->pixels);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  SDL_FreeSurface(surf);

  return texture;
}

int window_height() {
  return height;
}

int window_width() {
  return width;
}

}  // namespace render
