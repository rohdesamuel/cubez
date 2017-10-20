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

// State
render::qbRenderable render_state;

void initialize(const Settings& settings) {
  // Initialize events.
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
    glVertexAttribPointer(inTexCoord, 2, GL_FLOAT, GL_FALSE,
        5 * sizeof(float), (void*)(3 * sizeof(float)));

    render::Material material;
    material.shader_id = shaders.id();
    material.texture_id = render::load_texture(settings.texture);

    render_state = render::create(material, mesh);
  }

  std::cout << "Finished initializing ball\n";
}

void create(glm::vec3 pos, glm::vec3 vel) {
  qbEntityAttr attr;
  qb_entityattr_create(&attr);

  physics::Transform t{pos, vel};
  qb_entityattr_addcomponent(attr, physics::component(), &t);
  qb_entityattr_addcomponent(attr, render::component(), &render_state);

  qbEntity entity;
  qb_entity_create(&entity, attr);
  qb_entityattr_destroy(&attr);
}

}  // namespace ball
