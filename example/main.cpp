#define __ENGINE_DEBUG__
#include "inc/cubez.h"
#include "inc/table.h"
#include "inc/stack_memory.h"
#include "inc/schema.h"
#include "inc/timer.h"

#define GL3_PROTOTYPES 1

#include <GL/glew.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
//#include <GL/gl.h>
//#include <GL/glx.h>
//#include <GL/glu.h>

#include "ball.h"
#include "physics.h"
#include "player.h"
#include "log.h"

#include <thread>
#include <unordered_map>

const char vertex_shader[] =
    "#version 130\n"
    "in vec3 vp;"
    "out vec2 tex;"
    "void main() {"
    "  gl_Position = vec4(vp, 1.0);"
    "  tex = vec2(vp.x, vp.y);"
    "}";

const char fragment_shader[] =
    "#version 130\n"
    "in vec2 tex;"
    "out vec4 frag_color;"
    "void main() {"
    "  frag_color = vec4(1.0, 1.0, 0.0, 1.0);"
    "}";

SDL_Window *win = nullptr;
SDL_Renderer *renderer = nullptr;
SDL_GLContext *context = nullptr;

void init_rendering(int width, int height) {
  int posX = 100, posY = 100;
  
  SDL_Init(SDL_INIT_VIDEO);
  
  win = SDL_CreateWindow("Hello World", posX, posY, width, height, SDL_WINDOW_OPENGL);
  renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

  SDL_GL_SetAttribute(
      SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  
  SDL_GL_CreateContext(win);
}

GLuint create_shader(const char* shader, GLenum shader_type) {
  GLuint s = glCreateShader(shader_type);
  glShaderSource(s, 1, &shader, nullptr);
  glCompileShader(s);
  GLint success = 0;
  glGetShaderiv(s, GL_COMPILE_STATUS, &success);
  if (success == GL_FALSE) {
    GLint log_size = 0;
    glGetShaderiv(s, GL_INFO_LOG_LENGTH, &log_size);
    char* error = new char[log_size];

    glGetShaderInfoLog(s, log_size, &log_size, error);
    std::cout << "Error in shader compilation: " << error << std::endl;
    delete[] error;
  }
  return s;
}

int main(int, char* []) {
  init_rendering(800, 600);

  glewExperimental = GL_TRUE;
  GLenum glewError = glewInit();
  if (glewError != 0) {
    std::cout << "Failed to intialize Glew\n"
      << "Error code: " << glewError;
    return 1;
  }

  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);
  SDL_GL_SwapWindow(win);

  GLuint vs = create_shader(vertex_shader, GL_VERTEX_SHADER);
  GLuint fs = create_shader(fragment_shader, GL_FRAGMENT_SHADER);

  GLuint shader_program = glCreateProgram();
  glAttachShader(shader_program, fs);
  glAttachShader(shader_program, vs);
  glLinkProgram(shader_program);
  glUseProgram(shader_program);

  // Create and initialize the game engine.
  cubez::Universe uni;
  cubez::init(&uni);

  // Create the "main".
  cubez::create_program(kMainProgram); 
  
  cubez::EventPolicy policy;
  cubez::create_event(kMainProgram, kRender, policy);

  {
    physics::Settings settings;
    physics::initialize(settings);
  }

  {
    player::Settings settings;
    player::initialize(settings);
  }

  {
    ball::initialize();
    ball::create({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, "ball.bmp");
  }

  {
    logging::initialize();
  }

  cubez::Collection* c = physics::get_collection();

  GLuint points_buffer;
  glGenBuffers(1, &points_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, points_buffer);
  glBufferData(
      GL_ARRAY_BUFFER,
      c->count(c) * c->values.size,
      c->values.data(c), GL_DYNAMIC_DRAW);

  cubez::start();
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
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(win);
            SDL_Quit();
            cubez::stop();
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
      }

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
        physics::create(p, v);
      }
      cubez::loop();
      accumulator -= dt;
      t += dt;
    }
    update_timer.stop();
    update_timer.step();

    render_timer.start();
    glBufferData(GL_ARRAY_BUFFER, c->count(c) * c->values.size, nullptr, GL_DYNAMIC_DRAW);
    glBufferSubData(
        GL_ARRAY_BUFFER, 0,
        c->count(c) * c->values.size,
        c->values.data(c));

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, c->values.size, 0);
    glDrawArrays(GL_POINTS, 0, c->count(c));
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableVertexAttribArray(0);

    ball::render();
    cubez::flush_events(kMainProgram, kRender);

    {
      GLenum error = glGetError();
      if (error) {
        const GLubyte* error_str = gluErrorString(error);
        std::cout << "Error(" << error << "): " << error_str << std::endl;
      }
    }

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
      double total = 15 * 1e6;
      logging::out(
          "Frame " + std::to_string(frame) + "\n" +
          + "Utili: "  + std::to_string(100.0 * render_timer.get_avg_elapsed_ns() / total) + " : " + std::to_string(100.0 * update_timer.get_avg_elapsed_ns() / total) + "\n"
          + "Update FPS: " + std::to_string(1e9 / update_timer.get_avg_elapsed_ns()) + "\n"
          + "Render FPS: " + std::to_string(1e9 / render_timer.get_avg_elapsed_ns()) + "\n"
          + "Total FPS: " + std::to_string(1e9 / fps_timer.get_elapsed_ns()) + "\n"
          + "Accum: " + std::to_string(accumulator) + "\n\n");
    }
  }
}
