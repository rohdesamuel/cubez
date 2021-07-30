#include <cubez/cubez.h>
#include <cubez/utils.h>
#include <cubez/log.h>
#include <cubez/mesh.h>
#include <cubez/input.h>
#include <cubez/render.h>
#include <cubez/audio.h>
#include <cubez/render_pipeline.h>
#include <cubez/gui.h>
#include <cubez/async.h>
#include <cubez/socket.h>
#include <cubez/struct.h>
#include <cubez/sprite.h>
#include <cubez/draw.h>

#include "renderer.h"
#include "forward_renderer.h"
#include "renderer.h"
#include "pong.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <string>
#include <cglm/struct.h>

#define NOMINMAX
#include <winsock.h>

void initialize_universe(qbUniverse* uni) {
  uint32_t width = 1200;
  uint32_t height = 800;

  qbUniverseAttr_ uni_attr = {};
  uni_attr.title = "Cubez example";
  uni_attr.width = width;
  uni_attr.height = height;

  qbRendererAttr_ renderer_attr = {};
  renderer_attr.create_renderer = qb_defaultrenderer_create;
  renderer_attr.destroy_renderer = qb_defaultrenderer_destroy;
  uni_attr.renderer_args = &renderer_attr;

  qbAudioAttr_ audio_attr = {};
  audio_attr.sample_frequency = 44100;
  audio_attr.buffered_samples = 8192;
  uni_attr.audio_args = &audio_attr;

  qbScriptAttr_ script_attr = {};
  const char* entrypoint = "main.lua";
  script_attr.entrypoint = entrypoint;
  uni_attr.script_args = &script_attr;

  qbResourceAttr_ resource_attr = {};
  resource_attr.scripts = "scripts";
  resource_attr.fonts = "fonts";
  resource_attr.meshes = "models";
  uni_attr.resource_args = &resource_attr;

  qb_init(uni, &uni_attr);
}

const float PADDLE_WIDTH = 50.f;
const float PADDLE_HEIGHT = 200.f;
const float BALL_RADIUS = 50.f;

float paddle_center;
vec2s paddle_left{};
vec2s paddle_right{};

float rot = 0.f;
void on_update(uint64_t frame, qbVar) {
  if (qb_key_ispressed(qbKey::QB_KEY_W)) {
    paddle_left = glms_vec2_add(paddle_left, { 0.f, -10.f });
  }
  else if (qb_key_ispressed(qbKey::QB_KEY_S)) {
    paddle_left = glms_vec2_add(paddle_left, { 0.f, 10.f });
  }

  if (qb_key_ispressed(qbKey::QB_KEY_UP)) {
    paddle_right = glms_vec2_add(paddle_right, { 0.f, -10.f });
  }
  else if (qb_key_ispressed(qbKey::QB_KEY_DOWN)) {
    paddle_right = glms_vec2_add(paddle_right, { 0.f, 10.f });
  }

  if (qb_key_ispressed(qbKey::QB_KEY_SPACE)) {
    float constant = (float)(rand() % 1000) / 100.f;
    float linear = (float)(rand() % 1000) / 10000.f;
    float quadratic = (float)(rand() % 1000) / 1000000.f;
    float lightMax = 1.f;
    float radius =
      (-linear + std::sqrtf(linear * linear - 4 * quadratic * (constant - (256.0 / 1.0) * lightMax)))
      / (2 * quadratic);
    for (int i = 0; i < 32; ++i) {

      float x = (float)(rand() % qb_window_width());
      float y = (float)(rand() % qb_window_height());

      float r = (float)(rand() % 10000) / 10000.f;
      float g = (float)(rand() % 10000) / 10000.f;
      float b = (float)(rand() % 10000) / 10000.f;

      qb_light_point(i, { r, g, b }, { x, y, 0.f }, linear, quadratic, radius);
    }
  }
  rot += 0.01f;
  //paddle_right = glms_vec2_add(paddle_right, paddle_r_vel);
}

int main(int, char* []) {
  qbUniverse uni = {};
  srand(time(nullptr));

  initialize_universe(&uni);
  qb_start();

  qb_mouse_setshow(QB_FALSE);

  float aspect = (float)qb_window_width() / (float)qb_window_height();
  qbCamera ortho = qb_camera_ortho(0.f, (float)qb_window_width(), (float)qb_window_height(), 0.f, { 0.f, 0.f });
  qbCamera perspective = qb_camera_perspective(
    45.f, aspect, 10.f, 20000.f,
    { qb_window_width() / 2.f, qb_window_height() / 2.f, -900.f },
    { qb_window_width() / 2.f, qb_window_height() / 2.f, 0.f },
    { 0.f, -1.f, 0.f });

  vec3s ball{ (float)(qb_window_width() / 2), (float)(qb_window_height() / 2), 0.f };
  
  paddle_center = (float)(qb_window_height() / 2) - PADDLE_HEIGHT / 2;
  paddle_left = { PADDLE_WIDTH, paddle_center };
  paddle_right = { (float)(qb_window_width() - PADDLE_WIDTH * 2), paddle_center };

  qbLoopCallbacks_ callbacks{ .on_update = on_update };
  qbLoopArgs_ args{};

  int num_lights = 4;
  for (int i = 0; i < num_lights; ++i) {
    qb_light_enable(i, QB_LIGHT_TYPE_POINT);
  }

  qb_light_enable(0, QB_LIGHT_TYPE_DIRECTIONAL);

  float constant = 1.0;
  float linear = 0.0001;
  float quadratic = 0.00004;
  float lightMax = 1.f;
  float radius =
    (-linear + std::sqrtf(linear * linear - 4 * quadratic * (constant - (256.0 / 1.0) * lightMax)))
    / (2 * quadratic);

  for (int i = 0; i < num_lights; ++i) {

    float x = (float)(rand() % qb_window_width() * 1000.f);
    float y = (float)(rand() % qb_window_height() * 1000.f);

    float r = (float)(rand() % 10000) / 10000.f;
    float g = (float)(rand() % 10000) / 10000.f;
    float b = (float)(rand() % 10000) / 10000.f;

    qb_light_point(i, { r, g, b }, { x, y, 0.f }, linear, quadratic, radius);
  }

  qb_light_point(0, { 1.f, 0.f, 0.f }, { 25.f, 25.f, 0.f }, linear, quadratic, radius);
  qb_light_point(1, { 0.f, 1.f, 0.f }, { (float)qb_window_width() - 25.f, 25.f, 0.f }, linear, quadratic, radius);
  qb_light_point(2, { 0.f, 0.f, 1.f }, { (float)qb_window_width() - 25.f, (float)qb_window_height() - 25.f, 0.f }, linear, quadratic, radius);
  qb_light_point(3, { 1.f, 0.f, 1.f }, { 25.f, (float)qb_window_height() - 25.f, 0.f }, linear, quadratic, radius);

  qb_light_directional(0, { 1.f, 1.f, 1.f }, { 0.f, 0.f, 1.f }, 0.2f);

  int frame = 0;
  qbMaterial_ red_shiny{
    .albedo = {1.f, 0.1f, 0.1f},
    .metallic = 10.f,
  };

  qbMaterial_ green_shiny{
    .albedo = {0.1f, 1.f, 0.1f},
    .metallic = 10.f,
  };

  qbMaterial_ blue_shiny{
    .albedo = {0.1f, 0.1f, 1.f},
    .metallic = 10.f,
  };

  qbMaterial_ magenta_shiny{
    .albedo = {1.f, 0.1f, 1.f},
    .metallic = 10.f,
  };

  qbMaterial_ white_shiny{
    .albedo = {1.f, 1.f, 1.f},
    .metallic = 10.f,
  };
  
  qbMesh ship_mesh = qb_mesh_load("ship", "arrowhead.obj");
  

  qbMaterial_ default_material{};
  qbCamera camera = perspective;
  while (qb_loop(&callbacks, &args) != QB_DONE) {
    qbDrawCommands d = qb_draw_begin(camera);

    qb_draw_colorf(d, 1.f, 1.f, 1.f);

    qb_draw_material(d, &white_shiny);
    qb_draw_identity(d);
    qb_draw_translatef(d, 0.f, 0.f, 0.f);
    qb_draw_rotatef(d, rot, 0.f, 1.f, 0.f);
    qb_draw_cube(d, 50.f);

    //qb_draw_material(d, &green_shiny);
    qb_draw_identity(d);
    qb_draw_translatef(d, (float)qb_window_width() - 50.f, 0.f, 0.f);
    qb_draw_rotatef(d, rot, 0.f, 1.f, 0.f);
    qb_draw_sphere(d, 250.f);

    //qb_draw_material(d, &blue_shiny);
    qb_draw_identity(d);
    qb_draw_translatef(d, (float)qb_window_width() - 50.f, (float)qb_window_height() - 50.f, 0.f);
    qb_draw_rotatef(d, rot, 0.f, 1.f, 0.f);
    qb_draw_cube(d, 50.f);

    //qb_draw_material(d, &magenta_shiny);
    qb_draw_identity(d);
    qb_draw_translatef(d, 0.f, (float)qb_window_height() - 50.f, 0.f);
    qb_draw_rotatef(d, rot, 0.f, 1.f, 0.f);
    qb_draw_rotatef(d, M_PI_4, 1.f, 0.f, 0.f);
    qb_draw_rotatef(d, M_PI_4, 0.f, 1.f, 0.f);
    qb_draw_cube(d, 50.f);
      
    qb_draw_identity(d);
    qb_draw_translatef(d, paddle_left.x, paddle_left.y, 0.f);
    qb_draw_rotatef(d, rot, 0.f, 1.f, 0.f);
    qb_draw_circle(d, 100.f);

    qb_draw_identity(d);
    qb_draw_translatef(d, paddle_right.x, paddle_right.y, 0.f);
    qb_draw_rotatef(d, rot, 0.f, 1.f, 0.f);
    qb_draw_quad(d, PADDLE_WIDTH, PADDLE_HEIGHT);
    
    /*qb_draw_identity(d);
    qb_draw_translatef(d, -(float)qb_window_width(), -(float)qb_window_height(), 100.f);
    qb_draw_colorf(d, 1.f, 1.f, 1.f);
    qb_draw_quad(d, (float)qb_window_width() * 3.f, (float)qb_window_height() * 3.f);*/

    int mouse_x, mouse_y;
    qb_mouse_getposition(&mouse_x, &mouse_y);
    vec3s n = qb_camera_screentoworld(camera, { (float)mouse_x, (float)mouse_y });

    qbRay_ cam_ray = { .orig = camera->eye, .dir = n};
    qbRay_ plane = { .orig = vec3s{}, .dir = {0.f, 0.f, 1.f} };

    float t = 0.f;
    qb_ray_checkplane(&cam_ray, &plane, &t);
    ball = glms_vec3_add(camera->eye, glms_vec3_scale(n, t));

    qb_draw_material(d, &white_shiny);
    qb_draw_identity(d);
    qb_draw_translatef(d, ball.x, ball.y, 0.f);
    //qb_draw_rotatef(d, rot, 0.f, 0.f, 1.f);
    //qb_draw_translatef(d, 250.f, 0.f, 0.f);
    qb_draw_rotatef(d, rot, 0.f, 1.f, 0.f);    
    //qb_draw_sphere(d, 50.f);
    //qb_draw_circle(d, BALL_RADIUS);
    qb_draw_scalef(d, 100.f);
    qb_draw_mesh(d, ship_mesh);

    qb_draw_end(d);

    if (frame++ % 1000 == 0) {
      qbTiming_ timing_info;
      qb_timing(uni, &timing_info);

      qb_log(QB_INFO, "%f", (float)timing_info.render_fps);
    }
  }
}