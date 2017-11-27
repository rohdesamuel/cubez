#include "physics.h"

#include <atomic>
#include <iostream>

namespace physics {

// Components
qbComponent transforms_;
qbComponent collidable_;
qbComponent movable_;

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

    qb_component_create(&transforms_, attr);
    qb_componentattr_destroy(&attr);
  }
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, Collidable);

    qb_component_create(&collidable_, attr);
    qb_componentattr_destroy(&attr);
  }

  {
    qbEventAttr attr;
    qb_eventattr_create(&attr);
    qb_eventattr_setmessagetype(attr, Collision);
    qb_event_create(&collision_event_, attr);
    qb_eventattr_destroy(&attr);
  }

  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_setpriority(attr, QB_MIN_PRIORITY);
    qb_systemattr_addsource(attr, transforms_);
    qb_systemattr_addsink(attr, transforms_);
    qb_systemattr_setfunction(attr,
        [](qbElement* elements, qbCollectionInterface*, qbFrame*) {
          Transform t;
          qb_element_read(elements[0], &t);

          t.p += t.v;

          qb_element_write(elements[0]);
        });
    qb_system_create(&move_system, attr);
    qb_systemattr_destroy(&attr);
  }
  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_addsource(attr, collidable_);
    qb_systemattr_addsource(attr, collidable_);
    qb_systemattr_setjoin(attr, qbComponentJoin::QB_JOIN_CROSS);
    qb_systemattr_setfunction(attr,
        [](qbElement* e, qbCollectionInterface*, qbFrame*) {
          qbEntity e_a = qb_element_getentity(e[0]);
          qbEntity e_b = qb_element_getentity(e[1]);

          if (e_a == e_b) {
            return;
          }

          Transform* a;
          Transform* b;

          qb_entity_getcomponent(e_a, transforms_, &a);
          qb_entity_getcomponent(e_b, transforms_, &b);

          glm::vec3 r = a->p - b->p;

          if (glm::length(r) < 32.0f) {
            Collision event{e_a, e_b};
            qb_event_send(collision_event_, &event);
          }

          /*
          if (qb_element_getid(e[0]) == qb_element_getid(e[1])) {
            return;
          }

          glm::vec3 r = a.p - b.p;
          if (glm::abs(r.x) <= 0.0001f && glm::abs(r.y) <= 0.0001f) {
            return;
          }

          glm::vec3 n = glm::normalize(r);
          float d = glm::length(r);

          if (d < 0.05) {
            float p = glm::dot(a.v, n) - glm::dot(b.v, n);
            //a.v += r * 0.01f;
            //b.v -= r * 0.01f;
            a.v = a.v - (p * n);// * 0.15f;
            b.v = b.v + (p * n);// * 0.15f;
            std::cout << "bounce\n";
          }
          */

          // CollisionEvent event{
          //   qb_element_getentity(e[0]),
          //   qb_element_getentity(e[1])};
          // qb_event_sendto(collision_event_, physics::Collidable, &event);
        });

    qb_system_create(&collision_system, attr);
    qb_systemattr_destroy(&attr);
  }
  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_setpriority(attr, QB_MIN_PRIORITY);
    qb_systemattr_settrigger(attr, qbTrigger::EVENT);
    qb_systemattr_addsink(attr, transforms_);
    qb_systemattr_setcallback(attr,
        [](qbCollectionInterface* collections, qbFrame* frame) {
          qbCollectionInterface* from = &collections[0];
          Impulse* impulse = (Impulse*)frame->event;
          Transform* transform = (Transform*)from->by_id(from, impulse->key);
          transform->v += impulse->p;
        });
    qb_system_create(&impulse_system, attr);
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

void send_impulse(qbEntity entity, glm::vec3 p) {
  qbId id = qb_entity_getid(entity);

  Impulse message{id, p};
  qb_event_send(impulse_event, &message);
}

qbComponent component() {
  return transforms_;
}

qbComponent collidable() {
  return collidable_;
}

qbResult on_collision(qbSystem system) {
  return qb_event_subscribe(collision_event_, system);
}


}  // namespace physics
