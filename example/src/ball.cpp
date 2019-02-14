#include "ball.h"

#include <cubez/timer.h>

#include "physics.h"
#include "render.h"
#include "shader.h"
#include "mesh_builder.h"

#include <atomic>

#include <glm/gtc/matrix_transform.hpp>
#include <GL/glew.h>
#include <SDL2/SDL_opengl.h>

namespace ball {

// State
render::qbRenderable render_state;
render::qbRenderable exploded_render_state;
qbMesh mesh;
Mesh* collision_mesh;

struct Ball {
  int death_counter;
};

qbComponent ball_component;
qbComponent Explodable;

void initialize(const Settings& settings) {
  {
    std::cout << "Initialize ball textures and shaders\n";

    MeshBuilder builder = MeshBuilder::Sphere(64.0f, 22, 22);
    mesh = builder.BuildRenderable(qbRenderMode::QB_TRIANGLES);//settings.mesh;
    collision_mesh = new Mesh(builder.BuildMesh());//settings.collision_mesh;
    render_state = render::create(mesh, settings.material);
    exploded_render_state = render::create(mesh, settings.material_exploded);
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
        if (qb_instance_hascomponent(inst, Explodable) &&
            qb_instance_hascomponent(inst, physics::collision())) {
          physics::Transform* transform;
          qb_instance_getcomponent(inst, physics::component(), &transform);

          for (int i = 0; i < 0; ++i) {
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
    qb_systemattr_addmutable(attr, physics::component());
    // qb_systemattr_addshared(attr, physics::component());
    qb_systemattr_setjoin(attr, qbComponentJoin::QB_JOIN_INNER);
    qb_systemattr_setfunction(attr,
        [](qbInstance* insts, qbFrame*) {
          qbInstance inst = insts[0];

          Ball* ball;
          qb_instance_getmutable(inst, &ball);
          
          physics::Transform* t;
          qb_instance_getcomponent(inst, physics::component(), &t);
          //t->v.z -= 0.25f;

          if (qb_instance_hascomponent(inst, physics::collision())) {
            //qb_entity_removecomponent(qb_instance_getentity(inst), physics::collision());
          }

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
    qb_systemattr_addmutable(attr, ball_component);
    qb_systemattr_addmutable(attr, physics::collision());
    qb_systemattr_setjoin(attr, qbComponentJoin::QB_JOIN_INNER);
    qb_systemattr_setfunction(attr,
                              [](qbInstance*, qbFrame*) {
      //qbInstance inst = insts[0];
      //qb_entity_destroy(qb_instance_getentity(inst));
      //qb_entity_removecomponent(qb_instance_getentity(inst), physics::collision());
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
          physics::qbCollision* c = (physics::qbCollision*)frame->event;
          if (qb_entity_hascomponent(c->a, ball_component) &&
              qb_entity_hascomponent(c->b, ball_component)) {
            //qb_material_setcolor(material, glm::vec4(0.0, 1.0, 0.0, 0.0));
            //qb_entity_destroy(c->a);
          }
        });

    qbSystem system;
    qb_system_create(&system, attr);

    physics::on_collision(system);

    qb_systemattr_destroy(&attr);
  }

  std::cout << "Finished initializing ball\n";
}

void create(glm::vec3 pos, glm::vec3 vel, bool explodable, bool /** collidable */) {
  qbEntityAttr attr;
  qb_entityattr_create(&attr);

  Ball b;
  b.death_counter = 10000; // explodable ? 1000 : 100 + (rand() % 25) - (rand() % 10);
  qb_entityattr_addcomponent(attr, ball_component, &b);

  physics::Transform t{pos, vel, false};
  qb_entityattr_addcomponent(attr, physics::component(), &t);

  qb_entityattr_addcomponent(attr, render::component(), &render_state);

  /*if (collidable) {
    physics::qbCollidable c;
    c.collision_mesh = collision_mesh;
    qb_entityattr_addcomponent(attr, physics::collidable(), &c);
  }*/

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
