#include "physics.h"
#include "ball.h"
#include "player.h"
#include <atomic>
#include <iostream>

#include "collision_utils.h"
#include "mesh_builder.h"
#include "input.h"

namespace physics {

// Components
qbComponent transforms_;
qbComponent collidable_;
qbComponent movable_;
qbComponent collision_;

// Events
qbEvent collision_event_;
qbEvent impulse_event;

// Systems
qbSystem impulse_system;
qbSystem collision_system;
qbSystem move_system;

void initialize(const Settings&) {
  // Initialize collections.
  std::cout << "Initializing physics components\n";
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, physics::Transform);
    //qb_componentattr_setshared(attr);

    qb_component_create(&transforms_, attr);
    qb_componentattr_destroy(&attr);
  }
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, qbCollidable);

    qb_component_create(&collidable_, attr);
    qb_componentattr_destroy(&attr);
  }
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, qbCollision);

    qb_component_create(&collision_, attr);
    qb_componentattr_destroy(&attr);
  }

  {
    qbEventAttr attr;
    qb_eventattr_create(&attr);
    qb_eventattr_setmessagetype(attr, qbCollision);
    qb_event_create(&collision_event_, attr);
    qb_eventattr_destroy(&attr);
  }

  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_setpriority(attr, QB_MIN_PRIORITY);
    qb_systemattr_addmutable(attr, transforms_);
    qb_systemattr_setfunction(attr,
        [](qbInstance* insts, qbFrame*) {
          Transform* t;
          qb_instance_getmutable(insts[0], &t);
          if (!t->is_fixed) {
            t->p += t->v;
          }
        });
    qb_system_create(&move_system, attr);
    qb_systemattr_destroy(&attr);
  }
  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_addconst(attr, collidable_);
    qb_systemattr_addconst(attr, collidable_);
    qb_systemattr_setjoin(attr, qbComponentJoin::QB_JOIN_CROSS);
    qb_systemattr_setfunction(attr,
        [](qbInstance* insts, qbFrame*) {
          qbEntity e_a = qb_instance_getentity(insts[0]);
          qbEntity e_b = qb_instance_getentity(insts[1]);

          if (e_a == e_b) {
            return;
          }

          qbCollidable* c_a;
          qbCollidable* c_b;
          qb_instance_getconst(insts[0], &c_a);
          qb_instance_getconst(insts[1], &c_b);

          Mesh* mesh_a = c_a->collision_mesh;
          Mesh* mesh_b = c_b->collision_mesh;

          Transform* a;
          Transform* b;
          qb_instance_getcomponent(insts[0], transforms_, &a);
          qb_instance_getcomponent(insts[1], transforms_, &b);

          if (collision_utils::CollidesWith(a->p, *mesh_a, b->p, *mesh_b)) {
            if (glm::dot(b->p - a->p, b->p - a->p) != 0 && !a->is_fixed) {
              a->v -= glm::normalize(b->p - a->p);
            }

            qbCollision collision{e_a, e_b};
            qb_entity_addcomponent(e_a, physics::collision(), &collision);
            qb_entity_addcomponent(e_b, physics::collision(), &collision);
          }
        });
    qb_system_create(&collision_system, attr);
    qb_systemattr_destroy(&attr);
  }

  // Initialize events.
  {
    qbEventAttr attr;
    qb_eventattr_create(&attr);
    qb_eventattr_setmessagetype(attr, Impulse);
    qb_event_create(&impulse_event, attr);
    qb_event_subscribe(impulse_event, impulse_system);
    qb_eventattr_destroy(&attr);
  }
}

qbComponent component() {
  return transforms_;
}

qbComponent collidable() {
  return collidable_;
}

qbComponent collision() {
  return collision_;
}

qbResult on_collision(qbSystem system) {
  return qb_event_subscribe(collision_event_, system);
}


}  // namespace physics
