#include "physics.h"

#include <atomic>

namespace physics {

// Event names
const char kInsert[] = "physics_insert";
const char kImpulse[] = "physics_impulse";
const char kCollision[] = "physics_collision";

// Collections
qbComponent objects;

// Channels
qbEvent impulse_event;

// Systems
qbSystem impulse_system;
qbSystem collision_system;
qbSystem move_system;

void initialize(const Settings&) {
  // Initialize collections.
  {
    std::cout << "Initializing physics collections\n";

    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setprogram(&attr, kMainProgram);
    qb_componentattr_setdatasize(&attr, sizeof(physics::Transform));

    qbComponent objects;
    qb_component_create(&objects, "transforms", attr);
  }

  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_setprogram(&attr, kMainProgram);
    qb_systemattr_setpriority(&attr, QB_MIN_PRIORITY);
    qb_systemattr_addsource(&attr, objects);
    qb_systemattr_addsink(&attr, objects);
    qb_systemattr_setfunction(&attr,
        [](qbSystem, qbElement* elements, qbCollectionInterface* collections) {
          Transform* particle = (Transform*)elements[0].value;
          particle->p += particle->v;
          if (particle->p.x >  1.0f) { particle->p.x =  0.98f; particle->v.x *= -0.98; }
          if (particle->p.x < -1.0f) { particle->p.x = -0.98f; particle->v.x *= -0.98; }
          if (particle->p.y >  1.0f) { particle->p.y =  0.98f; particle->v.y *= -0.98; }
          if (particle->p.y < -1.0f) { particle->p.y = -0.98f; particle->v.y *= -0.98; }

          collections[0].update(&collections[0], &elements[0]);
        });
    qb_system_create(&move_system, attr);
    qb_systemattr_destroy(&attr);
  }
  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_setprogram(&attr, kMainProgram);
    qb_systemattr_setpriority(&attr, 0);
    qb_systemattr_addsource(&attr, objects);
    qb_systemattr_addsource(&attr, objects);
    qb_systemattr_addsink(&attr, objects);
    qb_systemattr_setjoin(&attr, qbComponentJoin::QB_JOIN_CROSS);
    qb_systemattr_setfunction(&attr,
        [](qbSystem, qbElement* elements, qbCollectionInterface* sinks) {
          qbCollectionInterface* transforms = &sinks[0];
          Transform& a = *(Transform*)elements[0].value;
          Transform& b = *(Transform*)elements[1].value;
          glm::vec3 r = a.p - b.p;
          if (glm::abs(r.x) <= 0.0001f && glm::abs(r.y) <= 0.0001f) {
            return;
          }

          glm::vec3 n = glm::normalize(r);
          float d = glm::length(r);

          if (d < 0.025) {
            float p = glm::dot(a.v, n) - glm::dot(b.v, n);
            //a.v += r * 0.01f;
            //b.v -= r * 0.01f;
            a.v = a.v - (p * n);// * 0.15f;
            b.v = b.v + (p * n);// * 0.15f;
          }

          transforms->update(transforms, &elements[0]);
          transforms->update(transforms, &elements[1]);
        });

    qb_system_create(&collision_system, attr);
  }
  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_setprogram(&attr, kMainProgram);
    qb_systemattr_setpriority(&attr, QB_MIN_PRIORITY);
    qb_systemattr_settrigger(&attr, qbTrigger::EVENT);
    qb_systemattr_addsink(&attr, objects);
    qb_systemattr_setcallback(&attr,
        [](qbSystem, void* message, qbCollectionInterface* collections) {
          qbCollectionInterface* from = &collections[0];
          Impulse* impulse = (Impulse*)message;
          Transform* transform = (Transform*)from->by_key(from, &impulse->key);
          transform->v += impulse->p;
        });
    qb_system_create(&impulse_system, attr);
    qb_systemattr_destroy(&attr);
  }

  // Initialize events.
  {
    qbEventAttr attr;
    qb_eventattr_create(&attr);
    qb_eventattr_setprogram(&attr, kMainProgram);
    qb_eventattr_setmessagesize(&attr, sizeof(Impulse));
    qb_event_create(&impulse_event, attr);
    qb_event_subscribe(impulse_event, impulse_system);
    qb_eventattr_destroy(&attr);
  }
}

void send_impulse(qbId key, glm::vec3 p) {
  Impulse message{key, p};
  qb_event_send(impulse_event, &message);
}

qbComponent component() {
  return objects;
}

}  // namespace physics
