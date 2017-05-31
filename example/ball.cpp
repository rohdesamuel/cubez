#include "ball.h"

#include "physics.h"

#include <atomic>
#include <SDL2/SDL_opengl.h>

using cubez::Channel;
using cubez::Frame;
using cubez::Pipeline;
using cubez::send_message;
using cubez::open_channel;

namespace ball {

// Event names
const char kInsert[] = "ball_insert";

// Collections
Objects::Table* objects;
cubez::Collection* objects_collection;

// Channels
Channel* insert_channel;
Channel* render_channel;

// Pipelines
Pipeline* insert_pipeline;
Pipeline* render_pipeline;

// State
std::atomic_int next_id;

const char vertex_shader[] = "";
const char fragment_shader[] = "";

GLuint vs, fs;
GLuint shader_program;
GLuint tex_quad;


void initialize() {
  // Initialize collections.
  {
    std::cout << "Intializing ball collections\n";
    objects = new Objects::Table();
    objects_collection = cubez::add_collection(kMainProgram, kCollection);
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

  // Initialize pipelines.
  {
    std::cout << "Intializing ball pipelines\n";
    cubez::ExecutionPolicy policy;
    policy.priority = cubez::MIN_PRIORITY;
    policy.trigger = cubez::Trigger::EVENT;

    insert_pipeline = cubez::add_pipeline(kMainProgram, nullptr, kCollection);
    insert_pipeline->transform = [](Pipeline*, Frame* f) {
      std::cout << "insert ball\n";
      cubez::Mutation* mutation = &f->mutation;
      mutation->mutate_by = cubez::MutateBy::INSERT;
      Objects::Element* msg = (Objects::Element*)f->message.data;
      Objects::Element* el =
        (Objects::Element*)(mutation->element);
      *el = *msg;
    };
    cubez::enable_pipeline(insert_pipeline, policy);
  }
  {
    cubez::ExecutionPolicy policy;
    policy.priority = 0;
    policy.trigger = cubez::Trigger::EVENT;

    render_pipeline = cubez::add_pipeline(kMainProgram, kCollection, kCollection);
    cubez::add_source(render_pipeline,
        (std::string(kMainProgram) + std::string("/") + std::string(physics::kCollection)).c_str());

    render_pipeline->transform = nullptr;
    render_pipeline->callback = [](Pipeline*, Frame*, const cubez::Collections* sources,
                                                         const cubez::Collections*) {
      cubez::Collection* ball_c = sources->collection[0];
      cubez::Collection* physics_c = sources->collection[1];

      /*
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, texture1);
      glUniform1i(glGetUniformLocation(ourShader.Program, "ourTexture1"), 0);

      glBindVertexArray(VAO);
      glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
      glBindVertexArray(0);  
      */


      uint64_t size = ball_c->count(ball_c);
      for (uint64_t i = 0; i < size; ++i) {
        Objects::Value& ball =
          *(Objects::Value*)ball_c->accessor(ball_c, cubez::IndexedBy::OFFSET, &i);

        physics::Objects::Value& pv = *(physics::Objects::Value*)physics_c->accessor(
            physics_c, cubez::IndexedBy::KEY, &ball.physics_id);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, ball.texture_id);
        glEnable(GL_TEXTURE_2D);
        glBegin(GL_TRIANGLES);
        glTexCoord2f(0.0f,0.0f);
        glVertex3f( 0.0f, 0.1f, 0.0f);
        glTexCoord2f(1.0f,0.0f);
        glVertex3f(-0.1f,-0.1f, 0.0f);
        glTexCoord2f(0.0f,1.0f);
        glVertex3f( 1.0f,-0.2f, 0.0f);
        glEnd();
        glDisable(GL_TEXTURE_2D);

        glBegin(GL_LINES);
        glColor3f(1.0f,0.0f,0.0f);
        glVertex3f(pv.p.x, pv.p.y, 0.0f);
        glVertex3f(pv.p.x + 0.1f, pv.p.y, 0.0f);

        glColor3f(0.0f,1.0f,0.0f);
        glVertex3f(pv.p.x, pv.p.y, 0.0f);
        glVertex3f(pv.p.x, pv.p.y + 0.1f, 0.0f);
        glEnd();
      }
    };
    cubez::enable_pipeline(render_pipeline, policy);
  }

  // Initialize events.
  {
    std::cout << "Intializing ball events\n";
    cubez::EventPolicy policy;
    policy.size = sizeof(Objects::Element);
    cubez::create_event(kMainProgram, kInsert, policy);
    cubez::subscribe_to(kMainProgram, kInsert, insert_pipeline);
    insert_channel = open_channel(kMainProgram, kInsert);
  }
  {
    cubez::subscribe_to(kMainProgram, kRender, render_pipeline);
    render_channel = open_channel(kMainProgram, kRender);
  }

  // Initialize rendering items.
  {
  }

  std::cout << "Finished initializing ball\n";
}

uint32_t load_texture(const std::string& file) {
  uint32_t texture;

  // Load the image from the file into SDL's surface representation
  SDL_Surface* surf = SDL_LoadBMP(file.c_str());
  if (!surf) { //If failed, say why and don't continue loading the texture
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

cubez::Id create(glm::vec3 pos, glm::vec3 vel, const std::string& file) {
  cubez::Id new_id = next_id;
  ++next_id;

  cubez::Message* msg = cubez::new_message(insert_channel);
  Objects::Element el;
  el.indexed_by = cubez::IndexedBy::KEY;
  el.key = new_id;


  /*
  GLfloat vertices[] = {
     // Positions         // Colors           // Texture Coords
     0.5f,  0.5f, 0.0f,   1.0f, 0.0f, 0.0f,   1.0f, 1.0f,   // Top Right
     0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f,   1.0f, 0.0f,   // Bottom Right
    -0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f,   // Bottom Left
    -0.5f,  0.5f, 0.0f,   1.0f, 1.0f, 0.0f,   0.0f, 1.0f    // Top Left 
  };*/



  Object obj;
  obj.texture_id = load_texture(file);
  obj.physics_id = physics::create(pos, vel);
  obj.vao_id = 0;
  el.value = obj;
  *(Objects::Element*)msg->data = el;
  cubez::send_message(msg);

  return new_id;
}

void render() {
  cubez::send_message(cubez::new_message(render_channel));
}

}  // namespace ball
