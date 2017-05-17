#include "inc/cubez.h"
#include "inc/stack_memory.h"

#include "particles.h"

cubez::Collection* add_particle_collection(const char* collection, uint64_t particle_count) {
  cubez::Collection* particles =
      cubez::add_collection(kMainProgram, collection);

  Particles::Table* table = new Particles::Table();
  table->keys.reserve(particle_count);
  table->values.reserve(particle_count);

  particles->collection = (uint8_t*)table;
  particles->copy = Particles::Table::default_copy;
  particles->mutate = Particles::Table::default_mutate;
  particles->count = Particles::Table::default_count;

  particles->keys.data = Particles::Table::default_keys;
  particles->keys.size = sizeof(Particles::Key);
  particles->keys.offset = 0;

  particles->values.data = Particles::Table::default_values;
  particles->values.size = sizeof(Particles::Value);
  particles->values.offset = 0;

  cubez::Pipeline* insert_particle = cubez::add_pipeline(kMainProgram, nullptr, kParticleCollection);
  insert_particle->callback = nullptr;
  insert_particle->select = nullptr;
  insert_particle->transform = [](cubez::Frame* f) {
    cubez::Mutation* mutation = &f->mutation;
    mutation->mutate_by = cubez::MutateBy::INSERT;
    Particles::Element* msg = (Particles::Element*)f->message.data;
    Particles::Element* el =
      (Particles::Element*)(mutation->element);
    *el = *msg;
  };
  cubez::ExecutionPolicy insert_exe_policy;
  insert_exe_policy.trigger = cubez::Trigger::EVENT;
  cubez::enable_pipeline(insert_particle, insert_exe_policy);

  cubez::EventPolicy insert_policy;
  insert_policy.size = sizeof(Particles::Element);
  cubez::create_event(kMainProgram, kInsertParticle, insert_policy);
  cubez::subscribe_to(kMainProgram, kInsertParticle, insert_particle);

  cubez::Channel* insert = cubez::open_channel(kMainProgram, kInsertParticle);
  for (uint32_t i = 0; i < particle_count; ++i) {
    glm::vec3 p{
      2 * (((float)(rand() % 1000) / 1000.0f) - 0.5f),
      2 * (((float)(rand() % 1000) / 1000.0f) - 0.5f),
      0
    };

    glm::vec3 v{
      ((float)(rand() % 1000) / 100000.0f) - 0.005f,
      ((float)(rand() % 1000) / 100000.0f) - 0.005f,
      0
    };

    cubez::Message* msg = cubez::new_message(insert);
    Particles::Element el;
    el.indexed_by = cubez::IndexedBy::KEY;
    el.key = i;
    el.value = {p, v};
    *(Particles::Element*)msg->data = el;
    cubez::send_message(msg);
    //table->insert(i, {p, v});
  }

  return particles;
}

void add_particle_pipeline(const char* collection) {
  cubez::Pipeline* pipeline =
    cubez::add_pipeline(kMainProgram, collection, collection);
  pipeline->select = nullptr;
  pipeline->transform = [](cubez::Frame* f) {
    Particles::Element* el = (Particles::Element*)f->mutation.element;

    //el->value.p.x += ((rand() % 100000) - 50000) / 5000000.0f;
    //el->value.p.y += ((rand() % 100000) - 50000) / 5000000.0f;
    el->value.v.y -= 0.0001f;
    el->value.p += el->value.v;
    if (el->value.p.x >  1.0f) { el->value.p.x =  1.0f; el->value.v.x *= -1; }
    if (el->value.p.x < -1.0f) { el->value.p.x = -1.0f; el->value.v.x *= -1; }
    if (el->value.p.y >  1.0f) { el->value.p.y =  1.0f; el->value.v.y *= -1; }
    if (el->value.p.y < -1.0f) { el->value.p.y = -1.0f; el->value.v.y *= -1; }
  };

  cubez::ExecutionPolicy policy;
  policy.priority = cubez::MAX_PRIORITY;
  policy.trigger = cubez::Trigger::LOOP;
  enable_pipeline(pipeline, policy);
}

