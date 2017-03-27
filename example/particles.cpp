#include "inc/cubez.h"
#include "inc/stack_memory.h"

#include "particles.h"

cubez::Collection* add_particle_collection(const char* collection, uint64_t particle_count) {
  cubez::Collection* particles =
      cubez::add_collection(kMainProgram, collection);

  Particles::Table* table = new Particles::Table();
  table->keys.reserve(particle_count);
  table->values.reserve(particle_count);

  for (uint64_t i = 0; i < particle_count; ++i) {
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

    table->insert(i, {p, v});
  }

  cubez::Copy copy =
      [](const uint8_t*, const uint8_t* value, uint64_t offset,
         cubez::Stack* stack) {
        cubez::Mutation* mutation = (cubez::Mutation*)stack->alloc(
            sizeof(cubez::Mutation) + sizeof(Particles::Element));
        mutation->element = (uint8_t*)(mutation + sizeof(cubez::Mutation));
        mutation->mutate_by = cubez::MutateBy::UPDATE;
        Particles::Element* el =
            (Particles::Element*)(mutation->element);
        el->offset = offset;
        new (&el->value) Particles::Value(
            *(Particles::Value*)(value) );
      };

  cubez::Mutate mutate =
      [](cubez::Collection* c, const cubez::Mutation* m) {
        Particles::Table* t = (Particles::Table*)c->collection;
        Particles::Element* el = (Particles::Element*)(m->element);
        t->values[el->offset] = std::move(el->value); 
      };

  particles->collection = (uint8_t*)table;
  particles->copy = copy;
  particles->mutate = mutate;
  particles->count = [](cubez::Collection* c) -> uint64_t {
    return ((Particles::Table*)c->collection)->size();
  };

  particles->keys.data = [](cubez::Collection* c) -> uint8_t* {
    return (uint8_t*)((Particles::Table*)c->collection)->keys.data();
  };
  particles->keys.size = sizeof(Particles::Key);
  particles->keys.offset = 0;

  particles->values.data = [](cubez::Collection* c) -> uint8_t* {
    return (uint8_t*)((Particles::Table*)c->collection)->values.data();
  };
  particles->values.size = sizeof(Particles::Value);
  particles->values.offset = 0;
  return particles;
}

void add_particle_pipeline(const char* collection) {
  cubez::Pipeline* pipeline =
    cubez::add_pipeline(kMainProgram, collection, collection);
  pipeline->select = nullptr;
  pipeline->transform = [](cubez::Stack* s) {
    Particles::Element* el =
        (Particles::Element*)((cubez::Mutation*)(s->top()))->element;
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

