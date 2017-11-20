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

struct Ball {
  int death_counter;
};

qbComponent ball_component;

void initialize(const Settings& settings) {
  {
    std::cout << "Initialize ball textures and shaders\n";
    render_state = render::create(settings.mesh, settings.material);
  }
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, Ball);
    qb_component_create(&ball_component, attr);

    qb_component_ondestroy(ball_component, +[](qbEntity, qbComponent, void*) {
          for (int i = 0; i < 5; ++i) {
            ball::create({(float)(rand() % 500) - 250.0f,
                          (float)(rand() % 500) - 250.0f,
                          (float)(rand() % 500) - 250.0f},
                         {((float)(rand() % 500) - 250.0f) / 250.0f,
                          ((float)(rand() % 500) - 250.0f) / 250.0f,
                          ((float)(rand() % 500) - 250.0f) / 250.0f});
          }
          std::cout << "Bye!\n";
        });
    qb_component_oncreate(ball_component, +[](qbEntity, qbComponent, void*) {
          std::cout << "Hi!\n";
        });
    qb_componentattr_destroy(&attr);
  }
  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_addsource(attr, ball_component);
    qb_systemattr_addsink(attr, ball_component);
    qb_systemattr_setfunction(attr,
        +[](qbElement* els, qbCollectionInterface*, qbFrame*) {
          Ball ball;
          qb_element_read(els[0], &ball);       
          --ball.death_counter;
          if (ball.death_counter < 0) {
            qbEntity e;
            qb_entity_find(&e, qb_element_getid(els[0]));
            qb_entity_destroy(&e);
          } else {
            qb_element_write(els[0]);
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

  Ball b;
  b.death_counter = rand() % 10000;
  qb_entityattr_addcomponent(attr, ball_component, &b);

  physics::Transform t{pos, vel, false};
  qb_entityattr_addcomponent(attr, physics::component(), &t);

  qbEntity entity;
  qb_entity_create(&entity, attr);

  qb_entity_addcomponent(entity, render::component(), &render_state);

  qb_entityattr_destroy(&attr);
}

}  // namespace ball
