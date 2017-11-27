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

    qb_component_ondestroy(ball_component, +[](qbEntity entity, qbComponent,
                                               void*) {
          physics::Transform* transform;
          qb_entity_getcomponent(entity, physics::component(), &transform);
          if (rand() % 5 == 0) {
            for (int i = 0; i < 5; ++i) {
              ball::create(transform->p,
                           {((float)(rand() % 500) - 250.0f) / 250.0f,
                            ((float)(rand() % 500) - 250.0f) / 250.0f,
                            ((float)(rand() % 500) - 250.0f) / 250.0f});
            }
          }
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
  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_settrigger(attr, qbTrigger::EVENT);
    qb_systemattr_setpriority(attr, QB_MAX_PRIORITY);
    qb_systemattr_setcallback(attr,
        +[](qbCollectionInterface*, qbFrame* frame) {
          physics::Collision* c = (physics::Collision*)frame->event;
          if (qb_entity_hascomponent(c->a, ball_component) == QB_OK &&
              qb_entity_hascomponent(c->b, ball_component) == QB_OK) {
            //qb_entity_removecomponent(c->a, physics::component());
            //qb_entity_destroy(&c->a);
            //qb_entity_destroy(&c->b);
          }
        });

    qbSystem system;
    qb_system_create(&system, attr);

    physics::on_collision(system);

    qb_systemattr_destroy(&attr);
  }

  std::cout << "Finished initializing ball\n";
}

void create(glm::vec3 pos, glm::vec3 vel) {
  qbEntityAttr attr;
  qb_entityattr_create(&attr);

  Ball b;
  b.death_counter = 100 + (rand() % 100) - (rand() % 100);
  qb_entityattr_addcomponent(attr, ball_component, &b);

  physics::Transform t{pos, vel, false};
  qb_entityattr_addcomponent(attr, physics::component(), &t);

  qb_entityattr_addcomponent(attr, render::component(), &render_state);

  physics::Collidable c;
  qb_entityattr_addcomponent(attr, physics::collidable(), &c);

  qbEntity entity;
  qb_entity_create(&entity, attr);


  qb_entityattr_destroy(&attr);
}

}  // namespace ball
