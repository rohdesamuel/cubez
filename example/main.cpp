#define __ENGINE_DEBUG__
#include "inc/cubez.h"
#include "inc/table.h"
#include "inc/timer.h"

#include "ball.h"
#include "physics.h"
#include "player.h"
#include "log.h"
#include "input.h"
#include "render.h"
#include "shader.h"

#include <thread>
#include <unordered_map>

const char tex_vs[] = R"(
#version 330 core
attribute vec3 inPos; 
attribute vec2 inTexCoord; 

uniform mat4 uMvp;

varying vec2 vTexCoord; 

void main() { 
  vTexCoord = inTexCoord; 
  gl_Position = uMvp * vec4(inPos, 1.0); 
}
)";

const char tex_fs[] = R"(
#version 330 core
uniform sampler2D uTexture; 

varying vec2 vTexCoord; 
 
void main() { 
  vec4 texColor = texture2D(uTexture, vTexCoord); 
  gl_FragColor = texColor; 
}
)";

const char simple_vs[] = R"(
#version 200

in vec3 inPos;
in vec3 inCol;

out vec3 vCol;

void main() {
  vCol = inCol;
  gl_Position = vec4(inPos, 1.0);
}
)";

const char simple_fs[] = R"(
#version 130

in vec3 vCol;
out vec4 frag_color;

void main() {
  frag_color = vec4(vCol, 1.0);
}
)";


void check_for_gl_errors() {
  GLenum error = glGetError();
  if (error) {
    const GLubyte* error_str = gluErrorString(error);
    std::cout << "Error(" << error << "): " << error_str << std::endl;
  }
}

void initialize_universe(qbUniverse* uni) {
  qb_init(uni);

  {
    logging::initialize();
  }

  {
    physics::Settings settings;
    physics::initialize(settings);
  }
  
  {
    render::Settings s;
    s.title = "Cubez example";
    s.width = 800;
    s.height = 600;
    s.znear = 1.0f;
    s.zfar = 1000.0f;
    s.fov = 90.0f;
    render::initialize(s);
    check_for_gl_errors();
  }
  
  {
    input::initialize();
  }

  {
    ball::Settings settings;
    settings.texture = "ball.bmp";
    settings.vs = tex_vs;
    settings.fs = tex_fs;

    ball::initialize(settings);

    ball::create({0.0f, 1.0f, 0.0f},
                 {0.0f, 0.0f, 0.0f});
    ball::create({1.0f, 0.0f, 0.0f},
                 {0.0f, 0.0f, 0.0f});
    check_for_gl_errors();
  }

  {
    player::Settings settings;
    settings.texture = "ball.bmp";
    settings.vs = tex_vs;
    settings.fs = tex_fs;
    settings.start_pos = {0, 0, 0};

    player::initialize(settings);
    check_for_gl_errors();
  }
}

int main(int, char* []) {
  // Create and initialize the game engine.
  qbUniverse uni;
  initialize_universe(&uni);

  qb_start();
  int frame = 0;
  WindowTimer fps_timer(50);
  WindowTimer update_timer(50);
  WindowTimer render_timer(50);

  glViewport(0, 0, 800, 600);

  double t = 0.0;
  const double dt = 0.01;
  double current_time = Timer::now() * 0.000000001;
  double start_time = Timer::now();
  double accumulator = 0.0;

  qb_loop();
  while (1) {
    fps_timer.start();

    double new_time = Timer::now() * 0.000000001;
    double frame_time = new_time - current_time;
    current_time = new_time;

    accumulator += frame_time;

    update_timer.start();
    while (accumulator >= dt) {
      SDL_Event e;
      if (SDL_PollEvent(&e)) {
        if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
          if (e.key.keysym.sym == SDLK_ESCAPE) {
            render::shutdown();
            SDL_Quit();
            qb_stop();
            exit(0);
          } else {
            SDL_Keycode key = e.key.keysym.sym;
            input::send_key_event(input::keycode_from_sdl(key),
                                  e.key.state == SDL_PRESSED);
          }
        }
      }

      qb_loop();
      accumulator -= dt;
      t += dt;
    }
    update_timer.stop();
    update_timer.step();

    render_timer.start();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    render::RenderEvent e;
    e.frame = frame;
    e.ftimestamp_us = Timer::now() - start_time;
    render::present(&e);

    check_for_gl_errors();

    render_timer.stop();
    render_timer.step();

    ++frame;
    fps_timer.stop();
    fps_timer.step();

    double time = Timer::now();

    static int prev_trigger = 0;
    static int trigger = 0;
    int period = 1;

    prev_trigger = trigger;
    trigger = int64_t(time - start_time) / 1000000000;

    if (trigger % period == 0 && prev_trigger != trigger) {
    //if (true && period && prev_trigger == prev_trigger && trigger == trigger) {
      double total = 15 * 1e6;
      logging::out(
          "Frame " + std::to_string(frame) + "\n" +
          + "Utili: "  + std::to_string(100.0 * render_timer.get_avg_elapsed_ns() / total)
          + " : " + std::to_string(100.0 * update_timer.get_avg_elapsed_ns() / total) + "\n"
          + "Render FPS: " + std::to_string(1e9 / render_timer.get_avg_elapsed_ns()) + "\n"
          + "Update FPS: " + std::to_string(1e9 / update_timer.get_avg_elapsed_ns()) + "\n"
          + "Total FPS: " + std::to_string(1e9 / fps_timer.get_elapsed_ns()) + "\n"
          + "Accum: " + std::to_string(accumulator) + "\n\n");
    }
  }
}
