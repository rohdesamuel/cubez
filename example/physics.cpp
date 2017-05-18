#include "physics.h"

#include <atomic>

using cubez::Channel;
using cubez::Pipeline;
using cubez::send_message;
using cubez::open_channel;

namespace physics {

// Event names
const char kInsert[] = "physics_insert";
const char kImpulse[] = "physics_impulse";
const char kCollision[] = "physics_collision";

// Collections
Objects::Table* objects;
cubez::Collection* objects_collection;

// Channels
Channel* insert_channel;
Channel* impulse_channel;

// Pipelines
Pipeline* impulse_pipeline;
Pipeline* collision_pipeline;
Pipeline* move_pipeline;
Pipeline* insert_pipeline;

// State
std::atomic_int next_id;

void initialize(const Settings&) {
  // Initialize collections.
  {
    objects = new Objects::Table();
    objects_collection = cubez::add_collection(kMainProgram, kCollection);
    objects_collection->collection = objects;

    objects_collection->accessor = Objects::Table::default_accessor;
    objects_collection->copy = Objects::Table::default_copy;
    objects_collection->mutate = Objects::Table::default_mutate;
    objects_collection->count = Objects::Table::default_count;

    objects_collection->keys.data = Objects::Table::default_keys;
    objects_collection->keys.size = sizeof(Objects::Key);
    objects_collection->keys.offset = 0;

    objects_collection->values.data = Objects::Table::default_values;
    objects_collection->values.size = sizeof(Objects::Value);
    objects_collection->values.offset = 0;
  }

  // Initialize pipelines.
  {
    cubez::ExecutionPolicy policy;
    policy.priority = cubez::MIN_PRIORITY;
    policy.trigger = cubez::Trigger::EVENT;

    insert_pipeline = cubez::add_pipeline(kMainProgram, nullptr, physics::kCollection);
    insert_pipeline->callback = nullptr;
    insert_pipeline->select = nullptr;
    insert_pipeline->transform = [](cubez::Frame* f) {
      cubez::Mutation* mutation = &f->mutation;
      mutation->mutate_by = cubez::MutateBy::INSERT;
      Objects::Element* msg = (Objects::Element*)f->message.data;
      Objects::Element* el =
        (Objects::Element*)(mutation->element);
      *el = *msg;
    };
    cubez::enable_pipeline(insert_pipeline, policy);
  }
  {
    cubez::ExecutionPolicy policy;
    policy.priority = cubez::MIN_PRIORITY;
    policy.trigger = cubez::Trigger::LOOP;

    move_pipeline = cubez::add_pipeline(kMainProgram, physics::kCollection, physics::kCollection);
    move_pipeline->callback = nullptr;
    move_pipeline->select = nullptr;
    move_pipeline->transform = [](cubez::Frame* f) {
      Objects::Element* el = (Objects::Element*)f->mutation.element;

      el->value.v.y -= 0.00001f;
      el->value.v *= 0.99f;
      el->value.p += el->value.v;
      if (el->value.p.x >  1.0f) { el->value.p.x =  1.0f; el->value.v.x *= -1; }
      if (el->value.p.x < -1.0f) { el->value.p.x = -1.0f; el->value.v.x *= -1; }
      if (el->value.p.y >  1.0f) { el->value.p.y =  1.0f; el->value.v.y *= -1; }
      if (el->value.p.y < -1.0f) { el->value.p.y = -1.0f; el->value.v.y *= -1; }
    };
    cubez::enable_pipeline(move_pipeline, policy);
  }
  {
    cubez::ExecutionPolicy policy;
    policy.priority = 0;
    policy.trigger = cubez::Trigger::LOOP;

    collision_pipeline = cubez::add_pipeline(kMainProgram, physics::kCollection, physics::kCollection);
    collision_pipeline->transform = nullptr;
    collision_pipeline->select = nullptr;
    collision_pipeline->callback = [](cubez::Frame*, const cubez::Collections*,
                                                   const cubez::Collections* sinks) {
      cubez::Collection* from = sinks->collection[0];
      uint64_t size = from->count(from);
      for (uint64_t i = 0; i < size; ++i) {
        for (uint64_t j = i + 1; j < size; ++j) {
          physics::Objects::Value& a =
              *(physics::Objects::Value*)from->accessor(from, cubez::IndexedBy::OFFSET, &i);
          physics::Objects::Value& b =
              *(physics::Objects::Value*)from->accessor(from, cubez::IndexedBy::OFFSET, &j);
          glm::vec3 r = b.p - a.p;
          float l = glm::length(r);
          if (l < 0.05) {
            a.v -= (r/l) * 0.001f;
            b.v += (r/l) * 0.001f;
          }
        }
      }
    };
    cubez::enable_pipeline(collision_pipeline, policy);
  }
  {
    cubez::ExecutionPolicy policy;
    policy.priority = cubez::MIN_PRIORITY;
    policy.trigger = cubez::Trigger::EVENT;

    impulse_pipeline = cubez::add_pipeline(kMainProgram, nullptr, physics::kCollection);
    impulse_pipeline->transform = nullptr;
    impulse_pipeline->select = nullptr;
    impulse_pipeline->callback = [](cubez::Frame* f, const cubez::Collections*,
                                                     const cubez::Collections* sinks) {
      cubez::Collection* from = sinks->collection[0];
      Impulse* impulse = (Impulse*)f->message.data;
      Objects::Value* value = (Objects::Value*)from->accessor(from, cubez::IndexedBy::KEY, &impulse->key);
      value->v += impulse->p;
    };
    cubez::enable_pipeline(impulse_pipeline, policy);
  }

  // Initialize events.
  {
    cubez::EventPolicy policy;
    policy.size = sizeof(Objects::Element);
    cubez::create_event(kMainProgram, kInsert, policy);
    cubez::subscribe_to(kMainProgram, kInsert, insert_pipeline);
    insert_channel = open_channel(kMainProgram, kInsert);
  }
  {
    cubez::EventPolicy policy;
    policy.size = sizeof(Impulse);
    cubez::create_event(kMainProgram, kImpulse, policy);
    cubez::subscribe_to(kMainProgram, kImpulse, impulse_pipeline);
    impulse_channel = open_channel(kMainProgram, kImpulse);
  }
}

cubez::Id create(glm::vec3 pos, glm::vec3 vel) {
    cubez::Id new_id = next_id;
    ++next_id;

    cubez::Message* msg = cubez::new_message(insert_channel);
    Objects::Element el;
    el.indexed_by = cubez::IndexedBy::KEY;
    el.key = new_id;
    el.value = {pos, vel};
    *(Objects::Element*)msg->data = el;
    cubez::send_message(msg);

    return new_id;
}

void send_impulse(Objects::Key key, glm::vec3 p) {
  cubez::Message* msg = cubez::new_message(impulse_channel);
  Impulse j;
  j.key = key;
  j.p = p;
  *(Impulse*)msg->data = j;
  cubez::send_message(msg);
}

cubez::Collection* get_collection() {
  return objects_collection;
}

}  // namespace physics
