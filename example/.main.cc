/**
* Author: Samuel Rohde (rohde.samuel@gmail.com)
*
* This file is subject to the terms and conditions defined in
* file 'LICENSE.txt', which is part of this source code package.
*/

#include <cstdint>

#include <iostream>
#include <sstream>

#include <string.h>
#include <time.h>

#include <SDL2/SDL.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <boost/unordered_map.hpp>

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#include <GL/glu.h>

#define __ENGINE_DEBUG__
#include "radiance.h"

#include "timer.h"
#include "vector_math.h"

#include "component.h"
#include "benchmark.h"

namespace radiance
{

Status init_opengl() {
  glewExperimental = GL_TRUE;
  GLenum glewError = glewInit();
  if (glewError != 0) {
    std::cout << "Failed to intialize Glew\n"
              << "Error code: " << glewError;
    return Status::FAILED_INITIALIZATION;
  }

  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                      SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  SDL_GL_SetSwapInterval(1);
  return Status::OK;
}

Status init_sdl(SDL_Window** window, SDL_GLContext* context) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::cout << "Failed to initialize SDL2\n"
              << SDL_GetError();
    return Status::FAILED_INITIALIZATION;
  }

  *window = SDL_CreateWindow(
      "Demo",
      SDL_WINDOWPOS_UNDEFINED,
      SDL_WINDOWPOS_UNDEFINED,
      640,
      480,
      SDL_WINDOW_OPENGL);

  if (!window) {
    std::cout << "Failed to create window\n"
              << SDL_GetError();
    return Status::FAILED_INITIALIZATION;
  }

  *context = SDL_GL_CreateContext(*window);
  return Status::OK;
}



}  // namespace radiance
#if 1
namespace program {
using namespace radiance;
class Program {
 public:
  struct Transformation {
    glm::vec3 p;
    glm::vec3 v;
  };
  typedef ::boost::container::vector<std::tuple<Handle, float>> Spring;
  typedef Schema<Handle, Spring> Springs;
  typedef Schema<Entity, Transformation> Transformations;

  typedef Pipeline<typename Springs::View,
                   typename Transformations::Table>
      SpringsPipeline;

  typedef Pipeline<typename Transformations::Table,
                   typename Transformations::Table>
      TransformationsPipeline;

  Program():
      springs_view(&springs),
      transformations_view(&transformations) {
    spring = System(&spring_system, std::ref(transformations_view));
    move = System(&move_system);
    bind_position = System(bind_position_system, 1.0f, -1.0f, -1.0f, 1.0f);

    springs_pipeline = SpringsPipeline(
          &springs_view,
          &transformations,
          {radiance::IndexedBy::KEY},
          {});
    springs_pipeline.add({spring});

    transformations_pipeline = TransformationsPipeline(
          &transformations,
          &transformations,
          {radiance::IndexedBy::OFFSET},
          {});
    //transformations_pipeline.add({move});
    transformations_pipeline.add({move, bind_position});
  }

  static void collide(const Transformation& me, const Transformation& other,
                      Transformation* accum) {
    glm::vec3 delta = other.p - me.p;
    float d = glm::length(delta);
    glm::vec3 mtd = delta * (((radius() * 2) - d) / d);

    float im1 = 1;
    float im2 = 1;
    float inv_mass = 1 / (im1 + im2);
    
    glm::vec3 v = other.v - me.v;
    float vn = glm::dot(v, glm::normalize(mtd));

    if (vn > 0.0f) {
      return;
    }

    const float restitution = 1.0f;
    float i = (-(1.0f + restitution) * vn) * inv_mass;
    glm::vec3 impulse = mtd * i;

    accum->v += impulse * im1;
  }
  
  static void spring_system(
      Frame* frame, Transformations::View& transformations_view) {
    auto* el = frame->result<Springs::View::Element>();

    Handle handle = el->key;

    const Transformation& me = transformations_view[handle];
    Transformation accum = me;

    for (const auto& connection : el->component) {
      const Handle& other_handle = std::get<0>(connection);
      const Transformation& other = transformations_view[other_handle];
      //const float& spring_k = std::get<1>(connection);
      glm::vec3 r = other.p - accum.p;
      float l = glm::length(r);
      if (l > radius()) {
        l = l * l;
        //accum.v += 0.0000000001f * (glm::normalize(r) / l);
      } else {
        collide(me, other, &accum);
        //accum.v -= 0.0000001f * (glm::normalize(r) / l);
      }

      /*
      if (glm::length(r) < 0.05f) {
        accum.v -= r * spring_k * 1.1f;
      }
      if (glm::length(r) < 0.2f) {
        accum.v += r * spring_k;
        accum.v *= 0.99999f;
      }*/
    }
    
    /*
    frame->result(Transformations::Mutation{
        MutateBy::WRITE,
        Transformations::Element{IndexedBy::HANDLE , handle, accum}
        });*/
    frame->result(Transformations::Element{IndexedBy::HANDLE , handle, accum});
  }

  static void move_system(Frame* frame) {
    auto* el = frame->result<Transformations::Element>();

    /*float l = el->component.v.length();
    el->component.v /= el->component.v.length();
    el->component.v *= std::min(l, 0.5f);*/
    //el->component.v.z -= 0.0001f;
    el->component.p += el->component.v;
  }

  static void bind_position_system(
      Frame* frame, float top, float left, float bottom, float right) {
    auto* el = frame->result<Transformations::Element>();
    if (el->component.p.x <= left || el->component.p.x >= right) {
      el->component.p.x = std::min(
          std::max(el->component.p.x, left), (float)(right));
      el->component.v.x *= -1.0f;
    }

    if (el->component.p.y <= bottom || el->component.p.y >= top) {
      el->component.p.y = std::min(
          std::max(el->component.p.y, bottom), (float)(top));
      el->component.v.y *= -1.0f;
    }

    if (el->component.p.z <= bottom || el->component.p.z >= top) {
      el->component.p.z = std::min(
          std::max(el->component.p.z, bottom), (float)(top));
      el->component.v.z *= -1.0f;
    }
  }

  System spring;
  System move;
  System bind_position;

  SpringsPipeline springs_pipeline;
  TransformationsPipeline transformations_pipeline;

  Springs::Table springs;
  Springs::View springs_view;
  Transformations::Table transformations;
  Transformations::View transformations_view;
  Transformations::MutationBuffer transformations_buffer;

  inline static constexpr float radius() {
    // return 0.1f;  // Makes a very tight galaxy
    return 0.25f; // Makes a good galaxy
  }
};
}  // namespace program

GLuint compile_shader(const char* program, GLenum shader_type) {
  GLuint s = glCreateShader(shader_type);
  glShaderSource(s, 1, &program, nullptr);
  glCompileShader(s);
  GLint success = 0;
  glGetShaderiv(s, GL_COMPILE_STATUS, &success);
  if (success == GL_FALSE) {
    GLint log_size = 0;
    glGetShaderiv(s, GL_INFO_LOG_LENGTH, &log_size);
    char* error = new char[log_size];
    glGetShaderInfoLog(s, log_size, &log_size, error);

    std::cout << "Error in shader compilation: " << error << std::endl;
    std::cout << "Shader program: \n" << program;
    delete[] error;
    exit(1);
  }
  return s;
}

void glPrintIfError() {
  GLenum error = glGetError();
  if (error) {
    std::cout << "Error: " << error << std::endl;
  }
}

int main() {
  SDL_Window* window = nullptr;
  SDL_GLContext context = nullptr;

  radiance::init_sdl(&window, &context);
  radiance::init_opengl();  

  GLuint triangle;
  glGenVertexArrays(1, &triangle);
  glBindVertexArray(triangle);

  static const GLfloat triangle_data[] = {
     0.0f,  0.5f, 0.0f,
     0.5f, -0.5f, 0.0f,
    -0.5f, -0.5f, 0.0f,
  };


  GLuint vertex_buffer;
  glGenBuffers(1, &vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, 9 * sizeof(float), triangle_data,
               GL_STATIC_DRAW);

  const char* vertex_shader =
      "#version 130\n"
      "in vec3 vp;"
      "uniform mat4 projection_view;"
      "void main() {"
      "  gl_Position = projection_view * vec4(vp, 1.0);"
      "}";

  const char* fragment_shader =
      "#version 130\n"
      "out vec4 frag_color;"
      "void main() {"
      "  frag_color = vec4(1.0, 0.0, 0.0, 1.0);"
      "}";

  GLuint vs = compile_shader(vertex_shader, GL_VERTEX_SHADER);
  GLuint fs = compile_shader(fragment_shader, GL_FRAGMENT_SHADER);

  glm::vec3 camera_center = {1.4, 0.8, 1.4};
  glm::vec3 camera_eye = {0, 0, 0};
  glm::vec3 camera_direction = glm::normalize(camera_eye - camera_center);
  glm::vec3 up = glm::normalize(glm::vec3{0, 0, 1});
  glm::vec3 camera_right = glm::cross(up, camera_direction);
  glm::vec3 camera_up = glm::cross(camera_direction, camera_right);
  glm::mat4 view = glm::lookAt(camera_center, camera_eye, camera_up);
  glm::mat4 projection = glm::perspective(
      glm::radians(90.0f),
      (float)640 / (float)480,
      0.1f,
      50.0f);
  glm::mat4 projection_view = projection * view;

  GLuint shader_program = glCreateProgram();

  glAttachShader(shader_program, fs);
  glAttachShader(shader_program, vs);
  glLinkProgram(shader_program);

  glClearColor(0.0, 0.0, 0.0, 1.0);

  uint64_t count = 1000;

  program::Program p;

  srand((uint32_t)time(NULL));
  for (uint64_t i = 0; i < count; ++i) {
    float x = 2 * (((GLfloat)(rand() % 10000) / 10000.0f) - 0.5f);
    float y = 2 * (((GLfloat)(rand() % 10000) / 10000.0f) - 0.5f);
    float z = 2 * (((GLfloat)(rand() % 10000) / 10000.0f) - 0.5f);
    glm::vec3 pos = {x, y, z};
    glm::vec3 vel = {x, y, z};
    vel *= 0.01f;
    p.transformations.insert(i, program::Program::Transformation{pos, vel});
  }
  
#if 1
  for (uint64_t i = 0; i < count; ++i) {
    program::Program::Springs::Component springs_list;
    for (uint64_t j = 0; j < count; ++j) {
      if (i != j) {
        springs_list.push_back(std::tuple<radiance::Handle, float>{j, 0.0001f});
      }
    }
    p.springs.insert(i, std::move(springs_list));
  }
#endif

  GLuint points_buffer;
  glGenBuffers(1, &points_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, points_buffer);
  glBufferData(
      GL_ARRAY_BUFFER,
      p.transformations.components.size() * sizeof(program::Program::Transformation),
      p.transformations.components.data(), GL_DYNAMIC_DRAW);

  bool running = true;
  uint64_t frame = 0;
  float camera_dir = 0;
  float camera_rad = 2.0f;
  float camera_zdir = 0;

  WindowTimer fps(60);
  while(running) {
    fps.start();
    p.springs_pipeline();
    p.transformations_buffer.flush(&p.transformations);
    p.transformations_pipeline();
    fps.stop();
    fps.step();

    static glm::vec2 mouse_pos;
    static glm::vec2 mouse_delta;

    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(shader_program);

    camera_center = glm::vec3{
      camera_rad * glm::cos(glm::radians(camera_dir)) * glm::sin(glm::radians(camera_zdir)),
      camera_rad * glm::sin(glm::radians(camera_dir)) * glm::sin(glm::radians(camera_zdir)),
      camera_rad * glm::cos(glm::radians(camera_zdir))
    };

    camera_direction = glm::normalize(camera_eye - camera_center);
    camera_right = glm::cross(up, camera_direction);
    camera_up = glm::cross(camera_direction, camera_right);
    view = glm::lookAt(camera_center, camera_eye, camera_up);

    projection_view = projection * view;
    GLuint projection_view_id = glGetUniformLocation(
        shader_program, "projection_view");
    glUniformMatrix4fv(projection_view_id, 1, GL_FALSE, &projection_view[0][0]);

    glBindBuffer(GL_ARRAY_BUFFER, points_buffer);
    glBufferSubData(
        GL_ARRAY_BUFFER, 0,
        p.transformations.size() * sizeof(program::Program::Transformation),
        p.transformations.components.data());

    glEnableClientState(GL_VERTEX_ARRAY);

    // 3 for 3 dimensions.
    glVertexPointer(3, GL_FLOAT, sizeof(program::Program::Transformation), (void*)(0));
    glDrawArrays(GL_POINTS, 0, p.transformations.size());
    glDisableClientState(GL_VERTEX_ARRAY);

    SDL_Event event;
    while(SDL_PollEvent(&event)) {
      switch(event.type) {
        case SDL_QUIT:
          running = false;
          break;
        case SDL_KEYDOWN:
          switch (event.key.keysym.sym) {
            case SDLK_ESCAPE:
              running = false;
              break;
          }
          break;
        case SDL_MOUSEMOTION:
          glm::vec2 pos = {event.motion.x, event.motion.y};
          mouse_delta = pos - mouse_pos;
          mouse_pos = pos;
          camera_dir += mouse_delta.x;
          camera_zdir = 180.0f * (pos.y / 480.0f);
          camera_zdir = std::max(std::min(camera_zdir, 179.9f), 0.1f);
          break;
      }

      GLenum error = glGetError();
      if (error) {
        const GLubyte* error_str = gluErrorString(error);
        std::cout << "Error(" << error << "): " << error_str << std::endl;
      }
    }
    SDL_GL_SwapWindow(window);
    ++frame;
    //printf("%.2fms           \r", fps.get_avg_elapsed_ns() / 1e6);
    printf("%.2ffps           \r", 1e9 / fps.get_avg_elapsed_ns());
  }

  glDeleteBuffers(1, &points_buffer);
  glDeleteBuffers(1, &vertex_buffer);

  SDL_GL_DeleteContext(context);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
#endif
#if 0
using namespace radiance;

struct Transformation {
  Vec3 p;
  Vec3 v;
};
typedef ::boost::container::vector<std::tuple<Handle, float>> Spring;

typedef Schema<Handle, Spring> Springs;
typedef Schema<Entity, Transformation> Transformations;

template<class Data_>
struct Attribute {
  typedef Data_ Type;
  Handle handle;
};

typedef Attribute<Transformations::Element> Movable;

struct Particle {
  struct has {
    Handle transformation;
  };

  struct is {
    Handle renderable;
    Handle movable;
  };
};

typedef radiance::Schema<Id, System> Renderables;
typedef radiance::Schema<Id, System> Movables;

int main() {
  const int MAX_HEIGHT = 640 / 8;
  const int MAX_WIDTH = 640 / 8;

  Springs::Table springs;
  Transformations::Table transformations;
  
  Id movable_id = 0;
  Movables::Table movables;
  movables.insert(movable_id, [](Frame* frame) {
    auto *el = frame->result<Movable::Type>();
    el->component.p += el->component.v;
  });

  Transformations::MutationBuffer transformations_queue;

  uint64_t count = 1000;

  for (uint64_t i = 0; i < count; ++i) {
    Vec3 p = {
      (float)(rand() % MAX_WIDTH), (float)(rand() % MAX_HEIGHT),
      0
    };
    Vec3 v;
    v.x = (((float)(rand() % 10000)) - 5000.0f) / 50000.0f;
    v.y = (((float)(rand() % 10000)) - 5000.0f) / 50000.0f;
    transformations_queue.emplace<MutateBy::INSERT, IndexedBy::OFFSET>(
      (int64_t)i, { p, v });
  }

  transformations_queue.flush(&transformations);
  srand((uint32_t)time(NULL));
  for (uint64_t i = 0; i < count; ++i) {
    for (uint64_t j = 0; j < count; ++j) {
      Springs::Component springs_list;
      springs_list.push_back(std::tuple<Handle, float>{ j, 0.0000001 });
      springs.insert(i, std::move(springs_list));
    }
  }

  Transformations::View transformations_view(&transformations);
  Springs::View springs_view(&springs);

  auto spring = System([=](Frame* frame) {
    auto* el = frame->result<Springs::View::Element>();

    Handle handle = el->key;
    Transformation accum = transformations_view[handle];

    for (const auto& connection : el->component) {
      const Handle& other = std::get<0>(connection);
      const float& spring_k = std::get<1>(connection);
      Vec3 p = transformations_view[other].p - transformations_view[handle].p;
      accum.v += p * spring_k;
    }
    
    frame->result(Transformations::Element{ IndexedBy::HANDLE , handle, accum});
  });

  auto move = System([=](Frame* frame) {
    auto* el = frame->result<Transformations::Element>();

    float l = el->component.v.length();
    el->component.v.normalize();
    el->component.v *= std::min(l, 10.0f);
    el->component.p += el->component.v;
  });

  auto bind_position = System(
    [](Frame* frame, int max_width, int max_height) {
      auto* el = frame->result<Transformations::Element>();

      if (el->component.p.x <= 0 || el->component.p.x >= max_width - 1) {
        el->component.p.x = std::min(
            std::max(el->component.p.x, 0.0f), (float)(max_width - 1));
        el->component.v.x *= -0.9f;
      }

      if (el->component.p.y <= 0 || el->component.p.y >= max_height - 1) {
        el->component.p.y = std::min(
            std::max(el->component.p.y, 0.0f), (float)(max_height - 1));
        el->component.v.y *= -0.9f;
      }
    }, MAX_WIDTH, MAX_HEIGHT);

  auto physics_pipeline = Transformations::make_system_queue();

  Pipeline<Springs::View, Transformations::Table> springs_pipeline(
      &springs_view,
      &transformations,
      {IndexedBy::KEY},
      {}, spring);

  Pipeline<Transformations::Table, Transformations::Table>
      transformations_pipeline(
          &transformations,
          &transformations,
          {IndexedBy::OFFSET},
          {}, bind_position * move);

  WindowTimer fps(60);
  uint64_t frame = 0;
  while (1) {
    fps.start();

    if (frame % 1 == 0) {
      physics_pipeline({ springs_pipeline, transformations_pipeline });
    } else {
      physics_pipeline({ transformations_pipeline });
    }
    
    fps.stop();
    fps.step();

    std::cout << frame << ": " << fps.get_avg_elapsed_ns()/1e6 << "\r";
    std::cout.flush();
    ++frame;
  }
  while (1);
}
#endif
#if 0
#include <getopt.h>

int main(int, char**) {
  Benchmark benchmark(1000000);
  benchmark.run();
  return 0;
}
#endif
#if 0

#define __ENGINE_DEBUG__
#include "radiance.h"
using radiance::Data;

struct Position {};
struct Velocity {};
struct Mesh {
  float** vertices;
};

struct Transformation {
  glm::vec3 v;
  glm::vec3 p;
};

typedef radiance::Schema<int32_t, Transformation> Transformations;

struct Moveable {
  static void move(radiance::Frame*) {}
};

struct Collidable {
};

template<class Source_, class Sink_>
radiance::Pipeline<Source_, Sink_> make_pipeline(
    radiance::Id source, radiance::Id sink, typename Source_::Reader reader,
    typename Sink_::Writer writer) {
  const Data& source_data = radiance::universe->data_manager.get(source);
  const Data& sink_data = radiance::universe->data_manager.get(sink);
  return radiance::Pipeline<Source_, Sink_>(
      (Source_*)source_data,
      (Sink_*)sink_data, reader, writer);
}

template<class Source_, class Sink_>
radiance::Pipeline<Source_, Sink_> make_pipeline(
    radiance::Data source, radiance::Data sink, typename Source_::Reader reader,
    typename Sink_::Writer writer) {
  return radiance::Pipeline<Source_, Sink_>(
      (Source_*)source.ptr,
      (Sink_*)sink.ptr, reader, writer);
}

int main() {
  using namespace radiance;
  Universe u;
  radiance::start(&u);

  DataManager& data_manager = universe->data_manager;
  Data table = data_manager.emplace<Transformations::Table>();
  Data view = data_manager.emplace<Transformations::View>(table);

  SystemManager& system_manager = universe->system_manager;
  std::cout << system_manager.add<Moveable>(Moveable::move) << std::endl;

  ComponentManager& component_manager = universe->component_manager;
  Id movable = component_manager.add<Transformations::Table>({table});
  Id collidable =  component_manager.add<Transformations::Table, Mesh>({});

  EntityManager entity_manager;
  Id ball = entity_manager.add({movable, collidable});

  auto program = make_pipeline<Transformations::View, Transformations::Table>(
      view, table,
      {radiance::IndexedBy::KEY}, {});
  program.add({ Moveable::move });

  radiance::stop(&u);
  return 0;
}

#endif
