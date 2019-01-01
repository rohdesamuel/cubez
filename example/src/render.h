#ifndef RENDER__H
#define RENDER__H

#include <string>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <set>
#include <unordered_map>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#include <cubez/cubez.h>
#include "mesh.h"

namespace render {

typedef struct qbRenderable_* qbRenderable;

struct RenderEvent {
  double alpha;
  uint64_t frame;
  uint64_t ftimestamp_us;
};

struct Settings {
  std::string title;
  int width;
  int height;
  float fov;
  float znear;
  float zfar;
};

void initialize(const Settings& settings);
void shutdown();
int window_height();
int window_width();

qbRenderable create(qbMesh mesh, qbMaterial material);

qbComponent component();

void add_transform(qbId renderable_id, qbId transform_id);

void render(qbId material, qbId mesg);

void present(RenderEvent* event);

qbResult qb_camera_setorigin(glm::vec3 new_origin);
qbResult qb_camera_setposition(glm::vec3 new_position);
qbResult qb_camera_setyaw(float new_yaw);
qbResult qb_camera_setpitch(float new_pitch);

qbResult qb_camera_incposition(glm::vec3 delta);
qbResult qb_camera_incyaw(float delta);
qbResult qb_camera_incpitch(float delta);

glm::vec3 qb_camera_getorigin();
glm::vec3 qb_camera_getposition();
glm::mat4 qb_camera_getorientation();
glm::mat4 qb_camera_getprojection();
glm::mat4 qb_camera_getinvprojection();

qbResult qb_camera_screentoworld(glm::vec2 screen, glm::vec3* world);

qbResult qb_render_makecurrent();
qbResult qb_render_makenull();

float qb_camera_getyaw();
float qb_camera_getpitch();
float qb_camera_getznear();
float qb_camera_getzfar();


}  // namespace render

#endif