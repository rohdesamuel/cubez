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
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/keysymdef.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glu.h>

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

Display                 *dpy;
Window                  root;
GLint                   att[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
XVisualInfo             *vi;
Colormap                cmap;
XSetWindowAttributes    swa;
Window                  win;
GLXContext              glc;
XWindowAttributes       gwa;

void init_rendering(int width, int height) {
  dpy = XOpenDisplay(NULL);

  if(dpy == NULL) {
    printf("\n\tcannot connect to X server\n\n");
    exit(0);
  }

  root = DefaultRootWindow(dpy);

  vi = glXChooseVisual(dpy, 0, att);

  if (vi == NULL) {
    printf("\n\tno appropriate visual found\n\n");
    exit(0);
  }

  cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);

  swa.colormap = cmap;
  swa.event_mask = ExposureMask | KeyPressMask;

  win = XCreateWindow(dpy, root, 0, 0, width, height, 0, vi->depth, InputOutput,
                      vi->visual, CWColormap | CWEventMask, &swa);

  XMapWindow(dpy, win);
  XFlush(dpy);
  XStoreName(dpy, win, "Cubez Example");

  glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
  glXMakeCurrent(dpy, win, glc);
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

  {
    physics::Settings settings;
    physics::initialize(settings);
  }

  {
    player::Settings settings;
    player::initialize(settings);
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

  XGetWindowAttributes(dpy, win, &gwa);
  glViewport(0, 0, gwa.width, gwa.height);

  double t = 0.0;
  const double dt = 0.01;
  double current_time = Timer::now() * 0.000000001;
  double accumulator = 0.0;

  std::unordered_map<char, bool> pressed_keys;
  XSelectInput(dpy, win, KeyPressMask | KeyReleaseMask);
  while (1) {
    while (XPending(dpy)) {
      XEvent xev;
      XNextEvent(dpy, &xev);
      switch (xev.type) {
        case KeyPress: {
          // 0x09 is ESC
          if (xev.xkey.keycode == 0x09) {
            glXMakeCurrent(dpy, None, NULL);
            glXDestroyContext(dpy, glc);
            XDestroyWindow(dpy, win);
            XCloseDisplay(dpy);
            cubez::stop();
            exit(0);
          } else {
            KeySym key = XLookupKeysym(&xev.xkey, 0);
            if (key == XK_w) {
              pressed_keys['w'] = true;
            } else if (key == XK_a) {
              pressed_keys['a'] = true;
            } else if (key == XK_s) {
              pressed_keys['s'] = true;
            } else if (key == XK_d) {
              pressed_keys['d'] = true;
            } else if (key == XK_space) {
              pressed_keys[' '] = true;
            }

          }
          break;
        } 
        case KeyRelease: {
          bool released = true;
          if (XEventsQueued(dpy, QueuedAfterReading)) {
            XEvent nev;
            XPeekEvent(dpy, &nev);

            if (nev.type == KeyPress && nev.xkey.time == xev.xkey.time &&
                nev.xkey.keycode == xev.xkey.keycode) {
              released = false;
              /* Key wasn’t actually released */
            }
          }
          
          if (released) {
              KeySym key = XLookupKeysym(&xev.xkey, 0);
              if (key == XK_w) {
                pressed_keys['w'] = false;
              } else if (key == XK_a) {
                pressed_keys['a'] = false;
              } else if (key == XK_s) {
                pressed_keys['s'] = false;
              } else if (key == XK_d) {
                pressed_keys['d'] = false;
              } else if (key == XK_space) {
                pressed_keys[' '] = false;
              }
            }
          break;
        }
        default:
          break;
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
    fps_timer.start();

    double new_time = Timer::now() * 0.000000001;
    double frame_time = new_time - current_time;
    current_time = new_time;

    accumulator += frame_time;

    update_timer.start();
    while (accumulator >= dt) {
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

    {
      GLenum error = glGetError();
      if (error) {
        const GLubyte* error_str = gluErrorString(error);
        std::cout << "Error(" << error << "): " << error_str << std::endl;
      }
    }

    glXSwapBuffers(dpy, win);
    render_timer.stop();
    render_timer.step();

    ++frame;
    fps_timer.stop();
    fps_timer.step();

    if (false && frame % 1000 == 0) {
      double total = 15 * 1e6;
      logging::out(
          "Frame " + std::to_string(frame) + "\n" +
          + "Utili: "  + std::to_string(100.0 * render_timer.get_avg_elapsed_ns() / total) + " : " + std::to_string(100.0 * update_timer.get_avg_elapsed_ns() / total) + "\n"
          + "Update FPS: " + std::to_string(1e9 / update_timer.get_avg_elapsed_ns()) + "\n"
          + "Render FPS: " + std::to_string(1e9 / render_timer.get_avg_elapsed_ns()) + "\n"
          + "Total FPS: " + std::to_string(1e9 / fps_timer.get_elapsed_ns()) + "\n"
          + "Accum: " + std::to_string(accumulator) + "\n");
    }
  }
}
