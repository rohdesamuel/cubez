#define __ENGINE_DEBUG__
#include "inc/cubez.h"
#include "inc/table.h"
#include "inc/timer.h"

#define GL3_PROTOTYPES 1

#include <GL/glew.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include "ball.h"
#include "physics.h"
#include "player.h"
#include "log.h"
#include "render.h"
#include "shader.h"
#include "generic.h"

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

SDL_Window *win = nullptr;
SDL_GLContext *context = nullptr;

void check_for_gl_errors() {
  GLenum error = glGetError();
  if (error) {
    const GLubyte* error_str = gluErrorString(error);
    std::cout << "Error(" << error << "): " << error_str << std::endl;
  }
}

void init_rendering(int width, int height) {
  int posX = 100, posY = 100;
  
  SDL_Init(SDL_INIT_VIDEO);
  
  // Request an OpenGL 3.3 context
  SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

  SDL_GL_SetAttribute(
      SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  win = SDL_CreateWindow("Hello World", posX, posY, width, height,
                         SDL_WINDOW_OPENGL);

  SDL_GL_CreateContext(win);

  glewExperimental = GL_TRUE;
  GLenum glewError = glewInit();
  if (glewError != 0) {
    std::cout << "Failed to intialize Glew\n"
      << "Error code: " << glewError;
    exit(1);
  }
  // Setting glewExperimental can cause an "INVALID_ENUM" OpenGL error. Swallow
  // that error here and carry on.
  glGetError();

  SDL_GL_SwapWindow(win);
}

void initialize_universe(qbUniverse* uni) {
  qb_init(uni);

  // Create the "main".
  qb_create_program(kMainProgram); 
  {
    logging::initialize();
  }

  {
    physics::Settings settings;
    physics::initialize(settings);
  }
  
  {
    render::initialize();
    check_for_gl_errors();
  }

  {
    //player::Settings settings;
    //player::initialize(settings);
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

#if 0
  {
    qbComponent transforms;
    {
      std::cout << "Initialize transforms component\n";
      qbComponentAttr attr;
      qb_componentattr_create(&attr);
      qb_componentattr_setprogram(&attr, kMainProgram);
      qb_componentattr_setdatasize(&attr, sizeof(physics::Transform));

      std::cout << "Creating transforms component\n";
      qb_component_create(&transforms, "transforms", attr);
    }

    qbEntity transform;
    {
      std::cout << "Initialize transform entity\n";
      qbEntityAttr attr;
      qb_entityattr_create(&attr);

      physics::Transform t{{0.0f, 0.0f, 0.0f}, {0.1f, 0.0f, 0.0f}};
      qb_entityattr_addcomponent(&attr, transforms, &t);

      for (int i = 0; i < 32; ++i) {
        std::cout << "entity: " << i << std::endl;
        qb_entity_create(&transform, attr);
      }
    }

    //Generic<Generic<int>, Generic<int>>::initialize(kMainProgram, "void");
    //Generic<void, Generic<int>, Generic<int>>::create(1, 1);
    //Generic<int, Generic<float>, Generic<int>>::initialize(kMainProgram, "intint");
    //Generic<int, Generic<float>, Generic<int>>::create(1, qbId(2), 3);
    //Generic<int, Generic<int>, Generic<int>>::create({1}, {2}, {3});
  }
#endif
}

int main(int, char* []) {
  init_rendering(800, 600);
  std::cout << "Using OpenGL " << glGetString(GL_VERSION) << std::endl;

  // Create and initialize the game engine.
  qbUniverse uni;
  initialize_universe(&uni);

  std::cout << "here\n";
  std::cout << "here\n";
  std::cout << "here\n";

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

  std::unordered_map<char, bool> pressed_keys;
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
            SDL_DestroyWindow(win);
            SDL_Quit();
            qb_stop();
            exit(0);
          } else {
            SDL_Keycode key = e.key.keysym.sym;
            bool state = e.key.state == SDL_PRESSED;
            if (key == SDLK_w) {
              pressed_keys['w'] = state;
            } else if (key == SDLK_a) {
              pressed_keys['a'] = state;
            } else if (key == SDLK_s) {
              pressed_keys['s'] = state;
            } else if (key == SDLK_d) {
              pressed_keys['d'] = state;
            } else if (key == SDLK_SPACE) {
              pressed_keys[' '] = state;
            }
          }
        }
      }

      /*
      if (pressed_keys['w']) {
        player::move_up(0.0001f);
      }

      if (pressed_keys['a']) {
        player::move_left(0.0001f);
      }

      if (pressed_keys['s']) {
        player::move_down(0.0001f);
      }

      if (pressed_keys['d']) {
        player::move_right(0.0001f);
      }*/

      if (pressed_keys[' ']) {
        glm::vec3 p{
          2 * (((float)(rand() % 1000) / 1000.0f) - 0.5f),
          2 * (((float)(rand() % 1000) / 1000.0f) - 0.5f),
          0
        };

        glm::vec3 v{
          ((float)(rand() % 1000) / 100000.0f) - 0.005f,
          ((float)(rand() % 1000) / 100000.0f) - 0.005f,
          0
        };
        ball::create(p, v);
      }
      qb_loop();
      accumulator -= dt;
      t += dt;
    }
    update_timer.stop();
    update_timer.step();

    render_timer.start();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /*
    render::RenderEvent e;
    e.frame = frame;
    e.ftimestamp_us = Timer::now() - start_time;
    render::present(&e);
    */

    check_for_gl_errors();

    SDL_GL_SwapWindow(win);

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
