#include "inc/radiance.h"
#include "inc/table.h"
#include "inc/stack_memory.h"
#include "inc/schema.h"
#include "inc/timer.h"

#include <glm/glm.hpp>
#include <omp.h>
#include <unordered_map>

typedef std::vector<glm::vec3> Vectors;

struct Transformation {
  glm::vec3 p;
  glm::vec3 v;
};

typedef radiance::Schema<uint32_t, Transformation> Transformations;

const char kMainProgram[] = "main";

int main() {

  uint64_t count = 4096;
  uint64_t iterations = 100000;
  std::cout << "Number of threads: " << omp_get_max_threads() << std::endl;
  std::cout << "Number of iterations: " << iterations << std::endl;
  std::cout << "Entity count: " << count << std::endl;

  radiance::Universe uni;
  radiance::init(&uni);

  radiance::create_program(kMainProgram); 
  radiance::Collection* transformations =
      radiance::add_collection(kMainProgram, "transformations");

  Transformations::Table* table = new Transformations::Table();
  table->keys.reserve(count);
  table->values.reserve(count);

  for (uint64_t i = 0; i < count; ++i) {
    glm::vec3 p{(float)i, 0, 0};
    glm::vec3 v{1, 0, 0};
    table->insert(i, {p, v});
  }

  radiance::Copy copy =
      [](const uint8_t*, const uint8_t* value, uint64_t offset,
         radiance::Stack* stack) {
        radiance::Mutation* mutation = (radiance::Mutation*)stack->alloc(
            sizeof(radiance::Mutation) + sizeof(Transformations::Element));
        mutation->element = (uint8_t*)(mutation + sizeof(radiance::Mutation));
        mutation->mutate_by = radiance::MutateBy::UPDATE;
        Transformations::Element* el =
            (Transformations::Element*)(mutation->element);
        el->offset = offset;
        new (&el->value) Transformations::Value(
            *(Transformations::Value*)(value) );
      };

  radiance::Mutate mutate =
      [](radiance::Collection* c, const radiance::Mutation* m) {
        Transformations::Table* t = (Transformations::Table*)c->collection;
        Transformations::Element* el = (Transformations::Element*)(m->element);
        t->values[el->offset] = std::move(el->value); 
      };

  transformations->collection = (uint8_t*)table;
  transformations->copy = copy;
  transformations->mutate = mutate;
  transformations->count = [](radiance::Collection* c) -> uint64_t {
    return ((Transformations::Table*)c->collection)->size();
  };

  transformations->keys.data = (uint8_t*)table->keys.data();
  transformations->keys.size = sizeof(Transformations::Key);
  transformations->keys.offset = 0;
  transformations->values.data = (uint8_t*)table->values.data();
  transformations->values.size = sizeof(Transformations::Value);
  transformations->values.offset = 0;

  radiance::Pipeline* pipeline =
      radiance::add_pipeline(kMainProgram, "transformations", "transformations");
  pipeline->select = nullptr;
  pipeline->transform = [](radiance::Stack* s) {
    Transformations::Element* el =
        (Transformations::Element*)((radiance::Mutation*)(s->top()))->element;
    el->value.p += el->value.v;
  };

  radiance::ExecutionPolicy policy;
  policy.priority = radiance::MAX_PRIORITY;
  policy.trigger = radiance::Trigger::LOOP;
  enable_pipeline(pipeline, policy);

  radiance::start();
  double total = 0.0;
  double avg = 0.0;
  Timer timer;
  for (uint64_t i = 0; i < iterations; ++i) {
    timer.start();
    radiance::loop();
    timer.stop();
    total += timer.get_elapsed_ns();
  }
  avg = total / iterations;
  std::cout << "elapsed ns: " << total << std::endl;
  std::cout << "avg ms per iteration: " << avg / 1e6 << std::endl;
  std::cout << "avg ns per iteration: " << avg << std::endl;
  std::cout << "avg ns per entity per iteration: " << avg / count << std::endl;
  std::cout << "iteration throughput: " << (1e9 / avg) << std::endl;
  std::cout << "entity throughput: " << count / (avg / 1e9) << std::endl;
  radiance::stop();

  return 0;
}
