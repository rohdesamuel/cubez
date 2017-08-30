#include "ball.h"

#include "physics.h"
#include "render.h"
#include "shader.h"
#include "inc/timer.h"

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

// Systems
qbSystem* insert_system;

// State
std::atomic_int next_id;
qbId render_id;

const char vertex_shader[] = "";
const char fragment_shader[] = "";

GLuint shader_program;
GLuint tex_quad;

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


void initialize(const Settings& settings) {
  // Initialize collections.
  next_id = 0;
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
    objects_collection->keys.stride = sizeof(Objects::Key);
    objects_collection->keys.offset = 0;

    objects_collection->values.data = Objects::Table::default_values;
    objects_collection->values.stride = sizeof(Objects::Value);
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
    std::cout << "Initialize ball textures and shaders\n";
    render::Mesh mesh;
    mesh.count = 6;
    GLfloat vertices[] = {
      0.5f,  0.5f, 0.0f,   1.0f, 1.0f,
      0.5f, -0.5f, 0.0f,   1.0f, 0.0f,
      -0.5f, -0.5f, 0.0f,   0.0f, 0.0f,

      -0.5f,  0.5f, 0.0f,   0.0f, 1.0f,
      0.5f,  0.5f, 0.0f,   1.0f, 1.0f,
      -0.5f, -0.5f, 0.0f,   0.0f, 0.0f,
    };

    glGenVertexArrays(1, &mesh.vao);
    glBindVertexArray(mesh.vao);

    glGenBuffers(1, &mesh.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
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

    render::Material material;
    material.shader_id = shaders.id();
    material.texture_id = load_texture(settings.texture);

    render_id = render::create(material, mesh);
  }

  std::cout << "Finished initializing ball\n";
}

qbId create(glm::vec3 pos, glm::vec3 vel) {
  qbId new_id = next_id;
  ++next_id;

  qbMessage* msg = qb_alloc_message(insert_channel);
  Objects::Element el;
  el.indexed_by = qbIndexedBy::KEY;
  el.key = new_id;


  Object obj;
  obj.physics_id = physics::create(pos, vel);
  obj.render_id = render_id;

  el.value = obj;
  *(Objects::Element*)msg->data = el;

  render::add_transform(render_id, obj.physics_id);

  qb_send_message(msg);

  return new_id;
}

}  // namespace ball
