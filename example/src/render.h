#ifndef CUBEZ_RENDER__H
#define CUBEZ_RENDER__H

#include <string>

#include <glm/glm.hpp>

#include <cubez/cubez.h>
#include "mesh.h"

typedef struct qbRenderable_* qbRenderable;

struct Settings {
  std::string title;
  int width;
  int height;
  float fov;
  float znear;
  float zfar;
};

typedef struct qbRenderEvent_ {
  double alpha;
  uint64_t frame;
} qbRenderEvent_, *qbRenderEvent;

qbRenderable create(qbMesh mesh, qbMaterial material);

qbComponent component();

void render_initialize(const Settings& settings);
void render_shutdown();
int window_height();
int window_width();

qbResult qb_render(qbRenderEvent event);

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

qbResult qb_render_swapbuffers();
qbResult qb_render_makecurrent();
qbResult qb_render_makenull();

float qb_camera_getyaw();
float qb_camera_getpitch();
float qb_camera_getznear();
float qb_camera_getzfar();

#endif  // CUBEZ_RENDER__H
