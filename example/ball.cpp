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

qbComponent ball_component;

void initialize(const Settings& settings) {
  {
    std::cout << "Initialize ball textures and shaders\n";
    render_state = render::create(settings.mesh, settings.material);
  }
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_component_create(&ball_component, attr);
    qb_componentattr_destroy(&attr);
  }
  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_addsource(attr, ball_component);
    qb_systemattr_addsource(attr, render::component());
    qb_systemattr_setjoin(attr, QB_JOIN_LEFT);
    qb_systemattr_setfunction(attr,
        +[](qbElement* els, qbCollectionInterface*, qbFrame*) {
          if (rand() % 10000 == 0) {
            qbEntity e;
            qb_entity_find(&e, qb_element_getid(els[0]));
            qb_entity_removecomponent(e, render::component());
          }
        });
    qbSystem s;
    qb_system_create(&s, attr);
    qb_systemattr_destroy(&attr);
  }

  std::cout << "Finished initializing ball\n";
}

void create(glm::vec3 pos, glm::vec3 vel) {
  qbEntityAttr attr;
  qb_entityattr_create(&attr);
  qb_entityattr_addcomponent(attr, ball_component, nullptr);
  physics::Transform t{pos, vel, false};
  qb_entityattr_addcomponent(attr, physics::component(), &t);

  qbEntity entity;
  qb_entity_create(&entity, attr);

  qb_entity_addcomponent(entity, render::component(), &render_state);

  qb_entityattr_destroy(&attr);
}

}  // namespace ball
