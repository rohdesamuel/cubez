#include "physics.h"

#include <atomic>

namespace physics {

// Event names
const char kInsert[] = "physics_insert";
const char kImpulse[] = "physics_impulse";
const char kCollision[] = "physics_collision";

// Collections
Objects::Table* objects;
qbCollection* objects_collection;

// Channels
qbChannel* insert_channel;
qbChannel* impulse_channel;

// Systems
qbSystem* impulse_system;
qbSystem* collision_system;
qbSystem* move_system;
qbSystem* insert_system;

// State
std::atomic_int next_id;

void initialize(const Settings&) {
  // Initialize collections.
  {
    objects = new Objects::Table();
    objects_collection = qb_alloc_collection(kMainProgram, kCollection);
    objects_collection->collection = objects;

    objects_collection->accessor = Objects::Table::default_accessor;
    objects_collection->copy = Objects::Table::default_copy;
    objects_collection->mutate = Objects::Table::default_mutate;
    objects_collection->count = Objects::Table::default_count;

    objects_collection->keys.data = Objects::Table::default_keys;
    objects_collection->keys.stride = sizeof(Objects::Key);
    objects_collection->keys.offset = 0;

    objects_collection->values.data = Objects::Table::default_values;
    objects_collection->values.stride = sizeof(Objects::Value);
    objects_collection->values.offset = 0;
  }

  // Initialize systems.
  {
    qbExecutionPolicy policy;
    policy.priority = QB_MIN_PRIORITY;
    policy.trigger = qbTrigger::EVENT;

    insert_system = qb_alloc_system(kMainProgram, nullptr, physics::kCollection);
    insert_system->transform = [](qbSystem*, qbFrame* f) {
      qbMutation* mutation = &f->mutation;
      mutation->mutate_by = qbMutateBy::INSERT;
      Objects::Element* msg = (Objects::Element*)f->message.data;
      Objects::Element* el =
        (Objects::Element*)(mutation->element);
      *el = *msg;
    };
    qb_enable_system(insert_system, policy);
  }
  {
    qbExecutionPolicy policy;
    policy.priority = QB_MIN_PRIORITY;
    policy.trigger = qbTrigger::LOOP;

    move_system = qb_alloc_system(kMainProgram, physics::kCollection, physics::kCollection);
    move_system->transform = [](qbSystem*, qbFrame* f) {
      Objects::Element* el = (Objects::Element*)f->mutation.element;

      //el->value.v.y -= 0.00001f;
      //el->value.v *= 0.99f;
      el->value.p += el->value.v;
      if (el->value.p.x >  1.0f) { el->value.p.x =  0.98f; el->value.v.x *= -0.98; }
      if (el->value.p.x < -1.0f) { el->value.p.x = -0.98f; el->value.v.x *= -0.98; }
      if (el->value.p.y >  1.0f) { el->value.p.y =  0.98f; el->value.v.y *= -0.98; }
      if (el->value.p.y < -1.0f) { el->value.p.y = -0.98f; el->value.v.y *= -0.98; }
    };
    qb_enable_system(move_system, policy);
  }
  {
    qbExecutionPolicy policy;
    policy.priority = 0;
    policy.trigger = qbTrigger::LOOP;

    collision_system = qb_alloc_system(kMainProgram, physics::kCollection, physics::kCollection);
    collision_system->transform = nullptr;
    collision_system->callback = [](
        qbSystem*, qbFrame*, const qbCollections*, const qbCollections* sinks) {
      qbCollection* from = sinks->collection[0];
      uint64_t size = from->count(from);
      for (uint64_t i = 0; i < size; ++i) {
        for (uint64_t j = i + 1; j < size; ++j) {
          physics::Objects::Value& a =
              *(physics::Objects::Value*)from->accessor(from, qbIndexedBy::OFFSET, &i);
          physics::Objects::Value& b =
              *(physics::Objects::Value*)from->accessor(from, qbIndexedBy::OFFSET, &j);

          glm::vec3 r = a.p - b.p;
          if (glm::abs(r.x) <= 0.0001f && glm::abs(r.y) <= 0.0001f) {
            continue;
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
        }
      }
    };
    qb_enable_system(collision_system, policy);
  }
  {
    qbExecutionPolicy policy;
    policy.priority = QB_MIN_PRIORITY;
    policy.trigger = qbTrigger::EVENT;

    impulse_system = qb_alloc_system(kMainProgram, nullptr, physics::kCollection);
    impulse_system->transform = nullptr;
    impulse_system->callback = [](qbSystem*, qbFrame* f, const qbCollections*,
                                                     const qbCollections* sinks) {
      qbCollection* from = sinks->collection[0];
      Impulse* impulse = (Impulse*)f->message.data;
      Objects::Value* value = (Objects::Value*)from->accessor(from, qbIndexedBy::KEY, &impulse->key);
      value->v += impulse->p;
    };
    qb_enable_system(impulse_system, policy);
  }

  // Initialize events.
  {
    qbEventPolicy policy;
    policy.size = sizeof(Objects::Element);
    qb_create_event(kMainProgram, kInsert, policy);
    qb_subscribe_to(kMainProgram, kInsert, insert_system);
    insert_channel = qb_open_channel(kMainProgram, kInsert);
  }
  {
    qbEventPolicy policy;
    policy.size = sizeof(Impulse);
    qb_create_event(kMainProgram, kImpulse, policy);
    qb_subscribe_to(kMainProgram, kImpulse, impulse_system);
    impulse_channel = qb_open_channel(kMainProgram, kImpulse);
  }
}

qbId create(glm::vec3 pos, glm::vec3 vel) {
    qbId new_id = next_id;
    ++next_id;

    qbMessage* msg = qb_alloc_message(insert_channel);
    Objects::Element el;
    el.indexed_by = qbIndexedBy::KEY;
    el.key = new_id;
    el.value = {pos, vel};
    *(Objects::Element*)msg->data = el;
    qb_send_message(msg);

    return new_id;
}

void send_impulse(Objects::Key key, glm::vec3 p) {
  qbMessage* msg = qb_alloc_message(impulse_channel);
  Impulse j;
  j.key = key;
  j.p = p;
  *(Impulse*)msg->data = j;
  qb_send_message(msg);
}

qbCollection* get_collection() {
  return objects_collection;
}

}  // namespace physics
