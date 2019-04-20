#include "pong.h"
#include "input.h"
#include "render_module.h"

#include <cubez/cubez.h>
#include <glm/ext.hpp>

namespace pong
{

qbRenderPass scene_pass;

qbComponent transform_component;

struct Paddle {
  glm::vec2 pos;
  glm::vec2 vel;
};

struct Ball {
  glm::vec2 pos;
  glm::vec2 vel;
};

// std140 layout
struct PaddleUniformModel {
  alignas(16) glm::mat4 modelview;
};

qbGpuBuffer top_paddle_ubo;
qbGpuBuffer bot_paddle_ubo;
qbGpuBuffer ball_ubo;

qbGpuBuffer vbo;
qbGpuBuffer ebo;

glm::vec2 paddle_size = { 128, 16 };
glm::vec2 ball_size = { 16, 16 };
uint32_t screen_width;
uint32_t screen_height;

Paddle top_paddle;
Paddle bot_paddle;
Ball ball;

void update_ubo(qbGpuBuffer ubo, glm::vec2 pos, glm::vec2 size) {
  glm::mat4 model = glm::mat4(1.0);
  model = glm::translate(model, glm::vec3(pos, 0));
  model = glm::translate(model, glm::vec3(0.5f * size.x, 0.5f * size.y, 0.0f));
  model = glm::translate(model, glm::vec3(-0.5f * size.x, -0.5f * size.y, 0.0f));
  model = glm::scale(model, glm::vec3(size, 1.0f));

  PaddleUniformModel uniform;
  uniform.modelview = model;
  qb_gpubuffer_update(ubo, 0, sizeof(PaddleUniformModel), &uniform.modelview);
}

bool CheckAabb(const glm::vec2& a_origin, const glm::vec2& b_origin,
               const glm::vec2& a_min, const glm::vec2& a_max,
               const glm::vec2& b_min, const glm::vec2& b_max) {
  return
    a_origin.x + a_min.x <= b_origin.x + b_max.x && a_origin.x + a_max.x >= b_origin.x + b_min.x &&
    a_origin.y + a_min.y <= b_origin.y + b_max.y && a_origin.y + a_max.y >= b_origin.y + b_min.y;
}

void Initialize(Settings settings) {
  scene_pass = settings.scene_pass;
  screen_width = settings.width;
  screen_height = settings.height;

  {
    qb_coro_sync([](qbVar) {
      float speed = 5;
      float dir = 90.0f;
      ball.vel.x = speed * glm::cos(dir);
      ball.vel.y = speed * glm::sin(dir);
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
          ball.vel.x = speed * glm::cos(dir);
          ball.vel.y = speed * glm::sin(dir);
          ball.pos =
            { (screen_width / 2) - (ball_size.x / 2),
              (screen_height / 2) - (ball_size.x / 2) };
        }

        if (CheckAabb(ball.pos, top_paddle.pos,
                      (ball_size *  0.0f), (ball_size * 1.0f),
                      (paddle_size* 0.0f), (paddle_size* 1.0f))) {
          glm::vec2 ball_o = ball.pos + (ball_size * 0.5f);
          glm::vec2 paddle_o = top_paddle.pos + (paddle_size * 0.5f);
          glm::vec2 r = glm::normalize(ball_o - paddle_o);
          glm::vec2 n = { 0.0f, 1.0f };
          ball.vel = r * speed;
          speed *= 1.04f;
        }

        if (CheckAabb(ball.pos, bot_paddle.pos,
                      (ball_size *  0.0f), (ball_size * 1.0f),
                      (paddle_size* 0.0f), (paddle_size* 1.0f)) &&
          ball.pos.y + ball_size.y / 2 < bot_paddle.pos.y + paddle_size.y / 2) {
          glm::vec2 ball_o = ball.pos + (ball_size * 0.5f);
          glm::vec2 paddle_o = bot_paddle.pos + (paddle_size * 0.5f);
          glm::vec2 r = glm::normalize(ball_o - paddle_o);
          glm::vec2 n = { 0.0f, -1.0f };
          ball.vel = r * speed;
          speed *= 1.04f;
        }

        ball.pos += ball.vel;
        top_paddle.pos += top_paddle.vel;
        bot_paddle.pos += bot_paddle.vel;

        glm::ivec2 mouse;
        input::get_mouse_position(&mouse.x, &mouse.y);

        bot_paddle.pos.x = mouse.x - paddle_size.x / 2;
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

  {
    float vertices[] = {
      // Positions        // Colors          // UVs
      0.0f, 0.0f, 0.0f,   1.0f, 1.0f, 1.0f,  0.0f, 0.0f,
      1.0f, 0.0f, 0.0f,   1.0f, 1.0f, 1.0f,  1.0f, 0.0f,
      1.0f, 1.0f, 0.0f,   1.0f, 1.0f, 1.0f,  1.0f, 1.0f,
      0.0f, 1.0f, 0.0f,   1.0f, 1.0f, 1.0f,  0.0f, 1.0f,
    };

    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = QB_GPU_BUFFER_TYPE_VERTEX;
    attr.data = vertices;
    attr.size = sizeof(vertices);
    attr.elem_size = sizeof(vertices[0]);
    qb_gpubuffer_create(&vbo, &attr);
  }
  {
    int indices[] = {
      3, 1, 0,
      3, 2, 1
    };

    qbGpuBufferAttr_ attr = {};
    attr.buffer_type = QB_GPU_BUFFER_TYPE_INDEX;
    attr.data = indices;
    attr.size = sizeof(indices);
    attr.elem_size = sizeof(indices[0]);
    qb_gpubuffer_create(&ebo, &attr);
  }

  {
    qbGpuBufferAttr_ attr;
    attr.buffer_type = qbGpuBufferType::QB_GPU_BUFFER_TYPE_UNIFORM;
    attr.data = nullptr;
    attr.size = sizeof(PaddleUniformModel);
    qb_gpubuffer_create(&top_paddle_ubo, &attr);
    top_paddle = {};
    top_paddle.pos = { (screen_width / 2) - (paddle_size.x / 2) , 100 };
  }
  {
    qbGpuBufferAttr_ attr;
    attr.buffer_type = qbGpuBufferType::QB_GPU_BUFFER_TYPE_UNIFORM;
    attr.data = nullptr;
    attr.size = sizeof(PaddleUniformModel);
    qb_gpubuffer_create(&bot_paddle_ubo, &attr);
    bot_paddle = {};
    bot_paddle.pos = { (screen_width / 2) - (paddle_size.x / 2), screen_height - 100 };
  }
  {
    qbGpuBufferAttr_ attr;
    attr.buffer_type = qbGpuBufferType::QB_GPU_BUFFER_TYPE_UNIFORM;
    attr.data = nullptr;
    attr.size = sizeof(PaddleUniformModel);
    qb_gpubuffer_create(&ball_ubo, &attr);
    ball = {};
    ball.pos =
      { (screen_width / 2) - (ball_size.x / 2),
        (screen_height / 2) - (ball_size.x / 2) };
  }

  {
    qbMeshBufferAttr_ attr = {};
    attr.bindings_count = qb_renderpass_bindings(scene_pass, &attr.bindings);
    attr.attributes_count = qb_renderpass_attributes(scene_pass, &attr.attributes);

    qbMeshBuffer top_paddle;
    qb_meshbuffer_create(&top_paddle, &attr);

    qb_meshbuffer_attachindices(top_paddle, ebo);

    qbGpuBuffer vertices[] = { vbo };
    qb_meshbuffer_attachvertices(top_paddle, vertices);

    qbGpuBuffer uniforms[] = { top_paddle_ubo };
    uint32_t bindings[] = { 1 };
    qb_meshbuffer_attachuniforms(top_paddle, 1, bindings, uniforms);

    qb_renderpass_append(scene_pass, top_paddle);
  }
  {
    qbMeshBufferAttr_ attr = {};
    attr.bindings_count = qb_renderpass_bindings(scene_pass, &attr.bindings);
    attr.attributes_count = qb_renderpass_attributes(scene_pass, &attr.attributes);

    qbMeshBuffer bot_paddle;
    qb_meshbuffer_create(&bot_paddle, &attr);

    qb_meshbuffer_attachindices(bot_paddle, ebo);

    qbGpuBuffer vertices[] = { vbo };
    qb_meshbuffer_attachvertices(bot_paddle, vertices);

    qbGpuBuffer uniforms[] = { bot_paddle_ubo };
    uint32_t bindings[] = { 1 };
    qb_meshbuffer_attachuniforms(bot_paddle, 1, bindings, uniforms);

    qb_renderpass_append(scene_pass, bot_paddle);
  }
  {
    qbMeshBufferAttr_ attr = {};
    attr.bindings_count = qb_renderpass_bindings(scene_pass, &attr.bindings);
    attr.attributes_count = qb_renderpass_attributes(scene_pass, &attr.attributes);

    qbMeshBuffer ball;
    qb_meshbuffer_create(&ball, &attr);

    qb_meshbuffer_attachindices(ball, ebo);

    qbGpuBuffer vertices[] = { vbo };
    qb_meshbuffer_attachvertices(ball, vertices);

    qbGpuBuffer uniforms[] = { ball_ubo };
    uint32_t bindings[] = { 1 };
    qb_meshbuffer_attachuniforms(ball, 1, bindings, uniforms);

    qb_renderpass_append(scene_pass, ball);
  }
}

void OnRender(qbRenderEvent event) {
  update_ubo(top_paddle_ubo, top_paddle.pos, paddle_size);
  update_ubo(bot_paddle_ubo, bot_paddle.pos, paddle_size);
  update_ubo(ball_ubo, ball.pos, ball_size);
}

}