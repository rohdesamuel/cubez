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
  {
    std::cout << "Initialize ball textures and shaders\n";
    render_state = render::create(settings.mesh, settings.material);
  }

  std::cout << "Finished initializing ball\n";
}

void create(glm::vec3 pos, glm::vec3 vel) {
  qbEntityAttr attr;
  qb_entityattr_create(&attr);

  physics::Transform t{pos, vel, false};
  qb_entityattr_addcomponent(attr, physics::component(), &t);
  qb_entityattr_addcomponent(attr, render::component(), &render_state);

  qbEntity entity;
  qb_entity_create(&entity, attr);
  qb_entityattr_destroy(&attr);
}

}  // namespace ball
