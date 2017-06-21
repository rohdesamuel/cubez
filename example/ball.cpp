#include "ball.h"

#include "physics.h"
#include "render.h"
#include "shader.h"

#include <atomic>

#include <glm/gtc/matrix_transform.hpp>
#include <GL/glew.h>
#include <SDL2/SDL_opengl.h>

namespace ball {

// Event names
const char kInsert[] = "ball_insert";

// Collections
Objects::Table* objects;
qbCollection* objects_collection;

// Channels
qbChannel* insert_channel;
qbChannel* render_channel;

// Systems
qbSystem* insert_system;
qbSystem* render_system;

// State
std::atomic_int next_id;

const char vertex_shader[] = "";
const char fragment_shader[] = "";

GLuint shader_program;
GLuint tex_quad;

void initialize(const Settings& settings) {
  // Initialize collections.
  {
    std::cout << "Intializing ball collections\n";
    objects = new Objects::Table();
    objects_collection = qb_alloc_collection(kMainProgram, kCollection);
    objects_collection->collection = objects;

    objects_collection->accessor = Objects::Table::default_accessor;
    objects_collection->copy = Objects::Table::default_copy;
    objects_collection->mutate = Objects::Table::default_mutate;
    objects_collection->count = Objects::Table::default_count;

    objects_collection->keys.data = Objects::Table::default_keys;
    objects_collection->keys.size = sizeof(Objects::Key);
    objects_collection->keys.offset = 0;

    objects_collection->values.data = Objects::Table::default_values;
    objects_collection->values.size = sizeof(Objects::Value);
    objects_collection->values.offset = 0;
  }

  // Initialize systems.
  {
    std::cout << "Intializing ball systems\n";
    qbExecutionPolicy policy;
    policy.priority = QB_MIN_PRIORITY;
    policy.trigger = qbTrigger::EVENT;

    insert_system = qb_alloc_system(kMainProgram, nullptr, kCollection);
    insert_system->transform = [](qbSystem*, qbFrame* f) {
      std::cout << "insert ball\n";
      qbMutation* mutation = &f->mutation;
      mutation->mutate_by = qbMutateBy::INSERT;
      Objects::Element* msg = (Objects::Element*)f->message.data;
      Objects::Element* el =
        (Objects::Element*)(mutation->element);
      *el = *msg;
    };
    qb_enable_system(insert_system, policy);
  }
  {
    qbExecutionPolicy policy;
    policy.priority = 0;
    policy.trigger = qbTrigger::EVENT;

    render_system = qb_alloc_system(kMainProgram, kCollection, kCollection);
    qb_add_source(render_system,
        (std::string(kMainProgram) + std::string("/") + std::string(physics::kCollection)).c_str());
    qb_add_source(render_system,
        (std::string(kMainProgram) + std::string("/") + std::string(render::kCollection)).c_str());

    render_system->transform = nullptr;
    render_system->callback = [](
        qbSystem*, qbFrame*,
        const qbCollections* sources,
        const qbCollections*) {

      qbCollection* ball_c = sources->collection[0];
      qbCollection* physics_c = sources->collection[1];
      qbCollection* render_c = sources->collection[2];

      uint64_t size = ball_c->count(ball_c);
      for (uint64_t i = 0; i < size; ++i) {
        Objects::Value& ball =
          *(Objects::Value*)ball_c->accessor(ball_c, qbIndexedBy::OFFSET, &i);

        physics::Objects::Value& pv =
            *(physics::Objects::Value*)physics_c->accessor(physics_c,
                qbIndexedBy::KEY, &ball.physics_id);

        RenderInfo& render_info = *(RenderInfo*)render_c->accessor(
            render_c, qbIndexedBy::KEY, &ball.render_id);

        glBindBuffer(GL_ARRAY_BUFFER, render_info.vbo);
        glBindVertexArray(render_info.vao);
        ShaderProgram shaders(render_info.shader_program);
        shaders.use();

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, ball.texture_id);
        
        glUniform1i(glGetUniformLocation(shaders.id(), "uTexture"), 0);
        glm::mat4 mvp = glm::ortho(0.0f, 8.0f, 0.0f, 6.0f);
        glm::vec3 p = pv.p;
        p = p + glm::vec3{1.0f, 1.0f, 0.0f};
        p.x *= 4.0f;
        p.y *= 3.0f;

        mvp = glm::translate(mvp, p);
        glUniformMatrix4fv(glGetUniformLocation(
              shaders.id(), "uMvp"), 1, GL_FALSE, (GLfloat*)&mvp);
        glDrawArrays(GL_TRIANGLES, 0, 6);
      }
    };
    qb_enable_system(render_system, policy);
  }

  // Initialize events.
  {
    std::cout << "Intializing ball events\n";
    qbEventPolicy policy;
    policy.size = sizeof(Objects::Element);
    qb_create_event(kMainProgram, kInsert, policy);
    qb_subscribe_to(kMainProgram, kInsert, insert_system);
    insert_channel = qb_open_channel(kMainProgram, kInsert);
  }

  {
    qb_subscribe_to(kMainProgram, kRender, render_system);
  }

  // Initialize rendering items.
  {
    RenderInfo render_info;

    GLfloat vertices[] = {
       0.5f,  0.5f, 0.0f,   1.0f, 1.0f,
       0.5f, -0.5f, 0.0f,   1.0f, 0.0f,
      -0.5f, -0.5f, 0.0f,   0.0f, 0.0f,

      -0.5f,  0.5f, 0.0f,   0.0f, 1.0f,
       0.5f,  0.5f, 0.0f,   1.0f, 1.0f,
      -0.5f, -0.5f, 0.0f,   0.0f, 0.0f,
    };

    glGenVertexArrays(1, &render_info.vao);
    glBindVertexArray(render_info.vao);

    glGenBuffers(1, &render_info.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, render_info.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    ShaderProgram shaders(settings.vs, settings.fs);
    shaders.use();

    GLint inPos = glGetAttribLocation(shaders.id(), "inPos");
    glEnableVertexAttribArray(inPos);
    glVertexAttribPointer(inPos, 3, GL_FLOAT, GL_FALSE,
                          5 * sizeof(float), 0);

    GLint inTexCoord = glGetAttribLocation(shaders.id(), "inTexCoord");
    glEnableVertexAttribArray(inTexCoord);
    glVertexAttribPointer(inTexCoord, 3, GL_FLOAT, GL_FALSE,
                          5 * sizeof(float), (void*)(3 * sizeof(float)));

    render_info.shader_program = shaders.id();
  }

  std::cout << "Finished initializing ball\n";
}

uint32_t load_texture(const std::string& file) {
  uint32_t texture;

  // Load the image from the file into SDL's surface representation
  SDL_Surface* surf = SDL_LoadBMP(file.c_str());

  if (!surf) {
    std::cout << "Could not load texture " << file << std::endl;
  }
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surf->w, surf->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surf->pixels);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  SDL_FreeSurface(surf);

  return texture;
}

qbId create(glm::vec3 pos, glm::vec3 vel,
    const std::string& tex,
    const std::string& vs,
    const std::string& fs) {
  qbId new_id = next_id;
  ++next_id;

  qbMessage* msg = qb_alloc_message(insert_channel);
  Objects::Element el;
  el.indexed_by = qbIndexedBy::KEY;
  el.key = new_id;

  RenderInfo render_info;

  GLfloat vertices[] = {
     0.5f,  0.5f, 0.0f,   1.0f, 1.0f,
     0.5f, -0.5f, 0.0f,   1.0f, 0.0f,
    -0.5f, -0.5f, 0.0f,   0.0f, 0.0f,

    -0.5f,  0.5f, 0.0f,   0.0f, 1.0f,
     0.5f,  0.5f, 0.0f,   1.0f, 1.0f,
    -0.5f, -0.5f, 0.0f,   0.0f, 0.0f,
  };

  glGenVertexArrays(1, &render_info.vao);
  glBindVertexArray(render_info.vao);

  glGenBuffers(1, &render_info.vbo);
  glBindBuffer(GL_ARRAY_BUFFER, render_info.vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  ShaderProgram shaders(vs, fs);
  shaders.use();

  GLint inPos = glGetAttribLocation(shaders.id(), "inPos");
  glEnableVertexAttribArray(inPos);
  glVertexAttribPointer(inPos, 3, GL_FLOAT, GL_FALSE,
                        5 * sizeof(float), 0);

  GLint inTexCoord = glGetAttribLocation(shaders.id(), "inTexCoord");
  glEnableVertexAttribArray(inTexCoord);
  glVertexAttribPointer(inTexCoord, 3, GL_FLOAT, GL_FALSE,
                        5 * sizeof(float), (void*)(3 * sizeof(float)));

  render_info.shader_program = shaders.id();

  Object obj;
  obj.texture_id = load_texture(tex);
  obj.physics_id = physics::create(pos, vel);
  obj.render_id = render::create(&render_info);
  el.value = obj;
  *(Objects::Element*)msg->data = el;
  qb_send_message(msg);

  return new_id;
}

}  // namespace ball
