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
qbComponent Explodable;

void initialize(const Settings& settings) {
  {
    std::cout << "Initialize ball textures and shaders\n";
    render_state = render::create(settings.mesh, settings.material);
  }
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_component_create(&Explodable, attr);
    qb_componentattr_destroy(&attr);
  }
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, Ball);
    qb_component_create(&ball_component, attr);
    qb_instance_ondestroy(ball_component, [](qbInstance inst) {
      //std::cout << "ball qb_instance_ondestroy\n";
        physics::Transform* transform;
        qb_instance_getcomponent(inst, physics::component(), &transform);
        if (qb_instance_hascomponent(inst, Explodable)) {
          for (int i = 0; i < 1000; ++i) {
            ball::create(transform->p,
                         {((float)(rand() % 500) - 250.0f) / 25.0f,
                          ((float)(rand() % 500) - 250.0f) / 25.0f,
                          ((float)(rand() % 500) - 250.0f) / 25.0f}, false, false);
          }
        }
      });
    qb_componentattr_destroy(&attr);
  }
  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_addmutable(attr, ball_component);
    qb_systemattr_setfunction(attr,
        [](qbInstance* insts, qbFrame*) {
          qbInstance inst = insts[0];

          Ball* ball;
          qb_instance_getmutable(inst, &ball);

          physics::Transform* t;
          qb_instance_getcomponent(inst, physics::component(), &t);
          t->v.z -= 0.25f;

          --ball->death_counter;
          if (ball->death_counter < 0 || (t->p.z <= -48 && qb_instance_hascomponent(inst, Explodable))) {
            //std::cout << "death by counter for " << qb_instance_getentity(inst) << "\n";
            qb_entity_destroy(qb_instance_getentity(inst));
          }
        });
    qbSystem s;
    qb_system_create(&s, attr);
    qb_systemattr_destroy(&attr);
  }
  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_settrigger(attr, qbTrigger::QB_TRIGGER_EVENT);
    qb_systemattr_setpriority(attr, QB_MAX_PRIORITY);
    qb_systemattr_setcallback(attr,
        [](qbFrame* frame) {
          physics::Collision* c = (physics::Collision*)frame->event;
          if (qb_entity_hascomponent(c->a, ball_component) &&
              qb_entity_hascomponent(c->b, ball_component)) {
            qb_entity_destroy(c->a);
          }
        });

    qbSystem system;
    qb_system_create(&system, attr);

    physics::on_collision(system);

    qb_systemattr_destroy(&attr);
  }

  std::cout << "Finished initializing ball\n";
}

void create(glm::vec3 pos, glm::vec3 vel, bool explodable, bool collidable) {
  qbEntityAttr attr;
  qb_entityattr_create(&attr);

  Ball b;
  b.death_counter = explodable ? 1 : 100 + (rand() % 25) - (rand() % 10);
  qb_entityattr_addcomponent(attr, ball_component, &b);

  physics::Transform t{pos, vel, false};
  qb_entityattr_addcomponent(attr, physics::component(), &t);

  qb_entityattr_addcomponent(attr, render::component(), &render_state);

  if (false) {
    physics::Collidable c;
    qb_entityattr_addcomponent(attr, physics::collidable(), &c);
  }

  if (explodable) {
    qb_entityattr_addcomponent(attr, Explodable, nullptr);
  }

  qbEntity entity;
  qb_entity_create(&entity, attr);


  qb_entityattr_destroy(&attr);
}

qbComponent Component() {
  return ball_component;
}

}  // namespace ball
