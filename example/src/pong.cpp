#include "pong.h"
#include <cubez/cubez.h>
#include <cubez/input.h>
#include <cubez/render_pipeline.h>
#include <cubez/render.h>
#include <cubez/mesh.h>
#include <cglm/struct.h>

namespace pong
{

qbRenderPass scene_pass;

qbComponent transform_component;

struct Paddle {
  vec2s pos;
  vec2s vel;
};

struct Ball {
  vec2s pos;
  vec2s vel;
};

// std140 layout
struct PaddleUniformModel {
  alignas(16) mat4s modelview;
};

qbGpuBuffer top_paddle_ubo;
qbGpuBuffer bot_paddle_ubo;
qbGpuBuffer ball_ubo;

qbGpuBuffer vbo;
qbGpuBuffer ebo;

vec2s paddle_size = { 128, 16 };
vec2s ball_size = { 16, 16 };
uint32_t screen_width;
uint32_t screen_height;

Paddle top_paddle;
Paddle bot_paddle;
Ball ball;

void update_ubo(qbGpuBuffer ubo, vec2s pos, vec2s size) {
  mat4s model = GLMS_MAT4_IDENTITY_INIT;
  model = glms_translate(model, vec3s{ pos.x, pos.y, 0 });
  model = glms_translate(model, vec3s{ 0.5f * size.x, 0.5f * size.y, 0.0f });
  model = glms_translate(model, vec3s{ -0.5f * size.x, -0.5f * size.y, 0.0f });
  model = glms_scale(model, vec3s{ size.x, size.y, 1.0f });

  PaddleUniformModel uniform;
  uniform.modelview = model;
  qb_gpubuffer_update(ubo, 0, sizeof(PaddleUniformModel), &uniform.modelview);
}

bool CheckAabb(const vec2s& a_origin, const vec2s& b_origin,
               const vec2s& a_min, const vec2s& a_max,
               const vec2s& b_min, const vec2s& b_max) {
  return
    a_origin.x + a_min.x <= b_origin.x + b_max.x && a_origin.x + a_max.x >= b_origin.x + b_min.x &&
    a_origin.y + a_min.y <= b_origin.y + b_max.y && a_origin.y + a_max.y >= b_origin.y + b_min.y;
}

void Initialize(Settings settings) {
  screen_width = settings.width;
  screen_height = settings.height;

  {
    qb_coro_sync([](qbVar) {
      float speed = 5;
      float dir = 90.0f;
      ball.vel.x = speed * cos(dir);
      ball.vel.y = speed * sin(dir);
      while (true) {
        if (ball.pos.x - (ball_size.x / 2) < 0) {
          ball.vel.x *= -1;
          ball.pos.x = (ball_size.x / 2.0f);
        }
        if (ball.pos.x + (ball_size.x / 2) > screen_width) {
          ball.vel.x *= -1;
          ball.pos.x = screen_width - (ball_size.x / 2.0f);
        }

        if (ball.pos.y < 0 ||
            ball.pos.y > screen_height) {
          float dir = 3.141592f * 0.5f;
          speed = 5.0f;
          ball.vel.x = speed * cos(dir);
          ball.vel.y = speed * sin(dir);
          ball.pos =
            { (screen_width / 2) - (ball_size.x / 2),
              (screen_height / 2) - (ball_size.x / 2) };
        }

        if (CheckAabb(ball.pos, top_paddle.pos, { 0.0f, 0.0f }, ball_size, { 0.0f, 0.0f }, paddle_size)) {
          vec2s ball_o = glms_vec2_add(ball.pos, glms_vec2_scale(ball_size, 0.5f));
          vec2s paddle_o = glms_vec2_add(top_paddle.pos, glms_vec2_scale(paddle_size, 0.5f));
          vec2s r = glms_vec2_normalize(glms_vec2_sub(ball_o, paddle_o));
          vec2s n = { 0.0f, 1.0f };
          ball.vel = glms_vec2_scale(r, speed);
          speed *= 1.04f;
        }

        if (CheckAabb(ball.pos, bot_paddle.pos, { 0.0f, 0.0f }, ball_size, { 0.0f, 0.0f }, paddle_size) &&
            ball.pos.y + ball_size.y / 2 < bot_paddle.pos.y + paddle_size.y / 2) {
          vec2s ball_o = glms_vec2_add(ball.pos, glms_vec2_scale(ball_size, 0.5f));
          vec2s paddle_o = glms_vec2_add(bot_paddle.pos, glms_vec2_scale(paddle_size, 0.5f));
          vec2s r = glms_vec2_normalize(glms_vec2_sub(ball_o, paddle_o));
          vec2s n = { 0.0f, -1.0f };
          ball.vel = glms_vec2_scale(r, speed);
          speed *= 1.04f;
        }

        ball.pos = glms_vec2_add(ball.pos, ball.vel);
        top_paddle.pos = glms_vec2_add(top_paddle.pos, top_paddle.vel);
        bot_paddle.pos = glms_vec2_add(bot_paddle.pos, bot_paddle.vel);

        int mouse_x, mouse_y;
        qb_get_mouse_position(&mouse_x, &mouse_y);

        bot_paddle.pos.x = mouse_x - paddle_size.x / 2;
        qb_coro_yield(qbNone);
      }
      return qbNone;
    }, qbNone);

    qb_coro_sync([](qbVar) {
      while (true) {
        if (ball.vel.y < 0) {
          top_paddle.vel.x = ((ball.pos.x + ball_size.x / 2) - (top_paddle.pos.x + paddle_size.x / 2)) * 0.15f;
        } else {
          top_paddle.vel.x = ((screen_width / 2) - (top_paddle.pos.x + paddle_size.x / 2)) * 0.01f;
        }
        qb_coro_waitframes(5);
      }
      return qbNone;
    }, qbNone);
  }

  qbEntity top_paddle;
  {
    qbMaterial material;
    {
      qbMaterialAttr_ attr = {};
      attr.albedo = { 1, 0, 0 };
      qb_material_create(&material, &attr, "");
    }

    qbTransform_ t = {};

    qbEntityAttr attr;
    qb_entityattr_create(&attr);
    //qb_entityattr_addcomponent(attr, qb_renderable(), qb_draw_rect(128, 16));
    //qb_entityattr_addcomponent(attr, qb_material(), &material);
    //qb_entityattr_addcomponent(attr, qb_transform(), &t);
    qb_entity_create(&top_paddle, attr);
    qb_entityattr_destroy(&attr);
  }

  /*
  qbEntity top_paddle;
  {
    qbMaterialAttr_ mattr = {};
    qbMaterial material;
    mattr.rgba = { 1, 0, 0, 1 };
    qb_material_create(&material, &mattr);

    qbEntityAttr attr;
    attr.components = {
      { qb_renderable, qb_draw_rect(128, 16) },
      { qb_material, material },
      { qb_transform, nullptr }
    };
    attr.components_size = 3;
    qb_entity_create(&top_paddle, attr);
  }
  */
}

void OnRender(qbRenderEvent event) {
  update_ubo(top_paddle_ubo, top_paddle.pos, paddle_size);
  update_ubo(bot_paddle_ubo, bot_paddle.pos, paddle_size);
  update_ubo(ball_ubo, ball.pos, ball_size);
}

}