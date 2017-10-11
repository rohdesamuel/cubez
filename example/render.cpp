#include "physics.h"
#include "render.h"
#include "shader.h"

#include "constants.h"

#include <atomic>
#include <glm/gtc/matrix_transform.hpp>
#include <GL/glew.h>
#include <SDL2/SDL_opengl.h>

namespace render {

struct qbRenderable_ {
  Material* material;
  Mesh* mesh;
};

// Collections
qbComponent renderables;

// Channels
qbEvent render_event;

// Systems
qbSystem render_system;

qbComponent component() {
  return renderables;
}

void render_event_handler(qbElement* elements, qbCollectionInterface*, qbFrame*) {
  qbRenderable renderable = (qbRenderable)elements[0].value;

  Material* material = renderable->material;
  Mesh* mesh = renderable->mesh;
  physics::Transform* transform = (physics::Transform*)elements[1].value;

  glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
  glBindVertexArray(mesh->vao);

  ShaderProgram shaders(material->shader_id);
  //shaders.use();

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, material->texture_id);
  glUniform1i(glGetUniformLocation(shaders.id(), "uTexture"), 0);
  glm::mat4 ortho = glm::ortho(0.0f, 8.0f, 0.0f, 6.0f);
  glm::mat4 mvp = ortho;
  glm::vec3 p = transform->p;
  p = p + glm::vec3{1.0f, 1.0f, 0.0f};
  p.x *= 4.0f;
  p.y *= 3.0f;

  mvp = glm::translate(mvp, p);
  glUniformMatrix4fv(glGetUniformLocation(
        shaders.id(), "uMvp"), 1, GL_FALSE, (GLfloat*)&mvp);
  glDrawArrays(GL_TRIANGLES, 0, mesh->count);
}

void present(RenderEvent* event) {
  qb_event_send(render_event, event);
  qb_event_flush(render_event);
}

qbRenderable create(const Material& mat, const Mesh& mesh) {
  return new qbRenderable_{new Material(mat), new Mesh(mesh)};
}

void initialize() {
  // Initialize collections.
  {
    std::cout << "Intializing renderables collection\n";
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setprogram(attr, kMainProgram);
    qb_componentattr_setdatasize(attr, sizeof(qbRenderable_));

    qb_component_create(&renderables, attr);
    qb_componentattr_destroy(&attr);
  }

  // Initialize systems.
  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_setprogram(attr, kMainProgram);
    qb_systemattr_settrigger(attr, qbTrigger::EVENT);
    qb_systemattr_setpriority(attr, QB_MIN_PRIORITY);
    qb_systemattr_addsource(attr, renderables);
    qb_systemattr_addsource(attr, physics::component());
    qb_systemattr_setjoin(attr, qbComponentJoin::QB_JOIN_INNER);
    qb_systemattr_setfunction(attr, render_event_handler);

    qb_system_create(&render_system, attr);
    qb_systemattr_destroy(&attr);
  }

  // Initialize events.
  {
    qbEventAttr attr;
    qb_eventattr_create(&attr);
    qb_eventattr_setprogram(attr, kMainProgram);
    qb_eventattr_setmessagesize(attr, sizeof(RenderEvent));

    qb_event_create(&render_event, attr);
    qb_event_subscribe(render_event, render_system);
    qb_eventattr_destroy(&attr);
  }

  std::cout << "Finished initializing render\n";
}

}  // namespace render
