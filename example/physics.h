#ifndef PHYSICS__H
#define PHYSICS__H

#include "constants.h"
#include "generic.h"

#include "inc/cubez.h"
#include "inc/schema.h"

#include <glm/glm.hpp>

namespace physics {


const char kCollection[] = "physics_objects";

struct Material {
  float mass;
  
  // Constant of restitution.
  float r;
};

struct Object {
  glm::vec3 p;
  glm::vec3 v;
};

struct Impulse {
  qbId key;
  glm::vec3 p;
};

struct Settings {
  float gravity = 0.0f;
  float friction = 0.0f;
};

typedef cubez::Schema<qbId, Object> Objects;

void initialize(const Settings& settings);
qbId create(glm::vec3 pos, glm::vec3 vel);
void send_impulse(Objects::Key key, glm::vec3 p);

qbCollection* get_collection();

struct Transform {
  glm::vec3 p;
  glm::vec3 v;
};

typedef Component<cubez::Table, qbId, Transform> Transforms;

class Physics: public Entity<Transforms> {
 public:
  static void move(qbSystem*, qbFrame* f) {
    Objects::Element* el = (Objects::Element*)f->mutation.element;

    //el->value.v.y -= 0.00001f;
    //el->value.v *= 0.99f;
    el->value.p += el->value.v;
    if (el->value.p.x >  1.0f) { el->value.p.x =  0.98f; el->value.v.x *= -0.98; }
    if (el->value.p.x < -1.0f) { el->value.p.x = -0.98f; el->value.v.x *= -0.98; }
    if (el->value.p.y >  1.0f) { el->value.p.y =  0.98f; el->value.v.y *= -0.98; }
    if (el->value.p.y < -1.0f) { el->value.p.y = -0.98f; el->value.v.y *= -0.98; }
  }

  static void on_collision(qbSystem*, qbFrame*, const qbCollections*,
                                                const qbCollections*);
  static void on_impulse(qbSystem*, qbFrame*, const qbCollections*,
                                              const qbCollections*);
};

}


#endif  // PHYSICS__H
