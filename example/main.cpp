#include "inc/cubez.h"
#include "inc/table.h"
#include "inc/stack_memory.h"
#include "inc/schema.h"
#include "inc/timer.h"

#define GL3_PROTOTYPES 1

#include <GL/glew.h>
#include<stdio.h>
#include<stdlib.h>
#include<X11/X.h>
#include<X11/Xlib.h>
#include<GL/gl.h>
#include<GL/glx.h>
#include<GL/glu.h>

#include "particles.h"

#include <unordered_map>

void add_render_pipeline() {
  cubez::Pipeline* render_pipeline =
      cubez::add_pipeline(kMainProgram, kParticleCollection, nullptr);
  render_pipeline->select = nullptr;
  render_pipeline->transform = [](cubez::Stack*){
   /* Particles::Element* el =
        (Particles::Element*)((cubez::Mutation*)(s->top()))->element;*/
  };

  render_pipeline->callback = [](cubez::Pipeline*, cubez::Collections, cubez::Collections) {
  };

  cubez::ExecutionPolicy policy;
  policy.priority = cubez::MIN_PRIORITY;
  policy.trigger = cubez::Trigger::LOOP;
  enable_pipeline(render_pipeline, policy);
}

Display                 *dpy;
Window                  root;
GLint                   att[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
XVisualInfo             *vi;
Colormap                cmap;
XSetWindowAttributes    swa;
Window                  win;
GLXContext              glc;
XWindowAttributes       gwa;
XEvent                  xev;

void init_rendering(int width, int height) {
  dpy = XOpenDisplay(NULL);

  if(dpy == NULL) {
    printf("\n\tcannot connect to X server\n\n");
    exit(0);
  }

  root = DefaultRootWindow(dpy);

  vi = glXChooseVisual(dpy, 0, att);

  if(vi == NULL) {
    printf("\n\tno appropriate visual found\n\n");
    exit(0);
  } 
  else {
    printf("\n\tvisual %p selected\n", (void *)vi->visualid);
  }

  cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);

  swa.colormap = cmap;
  swa.event_mask = ExposureMask | KeyPressMask;

  win = XCreateWindow(dpy, root, 0, 0, width, height, 0, vi->depth, InputOutput,
                      vi->visual, CWColormap | CWEventMask, &swa);

  XMapWindow(dpy, win);
  XStoreName(dpy, win, "Radiance Example");

  glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
  glXMakeCurrent(dpy, win, glc);
}

int main(int, char* []) {
  init_rendering(600, 600);

  glewExperimental = GL_TRUE;
  GLenum glewError = glewInit();
  if (glewError != 0) {
    std::cout << "Failed to intialize Glew\n"
      << "Error code: " << glewError;
    return 1;
  }

  const char* vertex_shader =
    "#version 130\n"
    "in vec3 vp;"
    "void main() {"
    "  gl_Position = vec4(vp, 1.0);"
    "}";

  const char* fragment_shader =
    "#version 130\n"
    "out vec4 frag_color;"
    "void main() {"
    "  frag_color = vec4(1.0, 0.0, 0.0, 1.0);"
    "}";

  GLuint vs = glCreateShader(GL_VERTEX_SHADER);
  {
    glShaderSource(vs, 1, &vertex_shader, nullptr);
    glCompileShader(vs);
    GLint success = 0;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
    if (success == GL_FALSE) {
      GLint log_size = 0;
      glGetShaderiv(vs, GL_INFO_LOG_LENGTH, &log_size);
      char* error = new char[log_size];

      glGetShaderInfoLog(vs, log_size, &log_size, error);
      std::cout << "Error in vertex shader compilation: " << error << std::endl;
      delete[] error;
    }
  }

  GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
  {
    glShaderSource(fs, 1, &fragment_shader, nullptr);
    glCompileShader(fs);
    GLint success = 0;
    glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
    if (success == GL_FALSE) {
      GLint log_size = 0;
      glGetShaderiv(vs, GL_INFO_LOG_LENGTH, &log_size);
      char* error = new char[log_size];

      glGetShaderInfoLog(vs, log_size, &log_size, error);
      std::cout << "Error in fragment shader compilation: " << error << std::endl;
      delete[] error;
    }
  }

  GLuint shader_program = glCreateProgram();
  glAttachShader(shader_program, fs);
  glAttachShader(shader_program, vs);
  glLinkProgram(shader_program);

  cubez::Universe uni;
  cubez::init(&uni);
  cubez::create_program(kMainProgram); 

  uint64_t particle_count = 100;
  cubez::Collection* c = add_particle_collection(kParticleCollection, particle_count);
  add_particle_pipeline(kParticleCollection);
  add_render_pipeline();

  GLuint points_buffer;
  glGenBuffers(1, &points_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, points_buffer);
  glBufferData(
      GL_ARRAY_BUFFER,
      c->count(c) * c->values.size,
      c->values.data, GL_DYNAMIC_DRAW);

  cubez::start();
  int frame = 0;
  WindowTimer frame_time(100);
  while (1) {
    frame_time.start();
    if (XCheckWindowEvent(dpy, win, KeyPressMask, &xev)) {
      if (xev.type == Expose) {
        XGetWindowAttributes(dpy, win, &gwa);
        glViewport(0, 0, gwa.width, gwa.height);
      } else if (xev.type == KeyPress) {
        glXMakeCurrent(dpy, None, NULL);
        glXDestroyContext(dpy, glc);
        XDestroyWindow(dpy, win);
        XCloseDisplay(dpy);
        cubez::stop();
        exit(0);
      }
    }
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(shader_program);

    glBindBuffer(GL_ARRAY_BUFFER, points_buffer);
    glBufferSubData(
        GL_ARRAY_BUFFER, 0,
        c->count(c) * c->values.size,
        c->values.data);

    glEnableClientState(GL_VERTEX_ARRAY);
    // 3 for 3 dimensions.
    glVertexPointer(3, GL_FLOAT, c->values.size, (void*)(0));
    glDrawArrays(GL_POINTS, 0, c->count(c));
    glDisableClientState(GL_VERTEX_ARRAY);

    GLenum error = glGetError();
    if (error) {
      const GLubyte* error_str = gluErrorString(error);
      std::cout << "Error(" << error << "): " << error_str << std::endl;
    }
    glXSwapBuffers(dpy, win);

    if (frame % 5 == 0) {
      cubez::loop();
    }
    ++frame;
    frame_time.stop();
    frame_time.step();

    std::cout << 1e9 / frame_time.get_elapsed_ns() << std::endl;
  }
}
