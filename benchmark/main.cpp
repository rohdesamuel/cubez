#include "inc/cubez.h"
#include "inc/timer.h"

#include <omp.h>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <glm/glm.hpp>

typedef std::vector<glm::vec3> Vectors;

struct PositionComponent {
  glm::vec2 p;
};

struct DirectionComponent {
  glm::vec2 dir;
};

struct ComflabulationComponent {
  float thingy;
  int dingy;
  bool mingy;
  std::string stringy;
};

qbComponent position_component;
qbComponent direction_component;
qbComponent comflabulation_component;

void move(qbElement* e, qbCollectionInterface*, qbFrame* f) {
  PositionComponent p;
  DirectionComponent d;
  qb_element_read(e[0], &p);
  qb_element_read(e[1], &d);

  float dt = *(float*)(f->state);
  p.p += d.dir * dt;

  qb_element_write(e[0]);
}

void comflab(qbElement* e, qbCollectionInterface*, qbFrame* f) {
  ComflabulationComponent comflab;
  qb_element_read(e[0], &comflab);
  comflab.thingy *= 1.000001f;
  comflab.mingy = !comflab.mingy;
  comflab.dingy++;
  qb_element_write(e[0]);
}

uint64_t create_entities_benchmark(uint64_t count) {
  Timer timer;
  qbEntityAttr attr;
  qb_entityattr_create(&attr);

  timer.start();
  for (uint64_t i = 0; i < count; ++i) {
    qbEntity entity;
    qb_entity_create(&entity, attr);
  }
  timer.stop();

  qb_entityattr_destroy(&attr);
  return timer.get_elapsed_ns();
#if 0
  PositionComponent pos;
  DirectionComponent dir;
  ComflabulationComponent com;

  qb_entityattr_addcomponent(attr, position_component, &pos);
  qb_entityattr_addcomponent(attr, direction_component, &dir);
  qb_entityattr_addcomponent(attr, comflabulation_component, &com);
#endif

}

template<class F>
void do_benchmark(const char* name, F f, uint64_t count, uint64_t iterations) {
  std::cout << "Running benchmark: " << name << "\n";
  uint64_t elapsed = 0;
  for (uint64_t i = 0; i < iterations; ++i) {
    elapsed += f(count);
  }
  std::cout << "Finished benchmark\n";
  std::cout << "Total elapsed: " << elapsed << "ns\n";
  std::cout << "Elapsed per iteration: " << elapsed / iterations << "ns\n";
}

int main() {
  qbUniverse uni;
  qb_init(&uni);
  qb_start();

  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, PositionComponent);
    qb_component_create(&position_component, attr);
    qb_componentattr_destroy(&attr);
  }
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, DirectionComponent);
    qb_component_create(&direction_component, attr);
    qb_componentattr_destroy(&attr);
  }
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, ComflabulationComponent);
    qb_component_create(&comflabulation_component, attr);
    qb_componentattr_destroy(&attr);
  }

  uint64_t count = 10'000'000;
  uint64_t iterations = 1;
  do_benchmark("Create Entities Benchmark",
               create_entities_benchmark, count, iterations);
  
  qb_stop();

   while (1);
}

#if 0
int main() {

  uint64_t count = 100000;
  uint64_t iterations = 10000;
  std::cout << "Number of threads: " << omp_get_max_threads() << std::endl;
  std::cout << "Number of iterations: " << iterations << std::endl;
  std::cout << "Entity count: " << count << std::endl;

  qbUniverse uni;
  qb_init(&uni);
  
  {
  	qbComponentAttr attr;
  	qb_componentattr_create(&attr);
  	qb_componentattr_setdatatype(attr, Transformation);
  	qb_component_create(&transformations, attr);
  	qb_componentattr_destroy(&attr);
  }
  
  qbSystem benchmark;
  {
  	qbSystemAttr attr;
  	qb_systemattr_create(&attr);
  	qb_systemattr_addsource(attr, transformations);
  	qb_systemattr_addsink(attr, transformations);
  	qb_systemattr_setfunction(attr,
        [](qbElement* element, qbCollectionInterface*, qbFrame*){
          Transformation t;
          qb_element_read(element[0], &t);
          
          //qbEntity e;
          //qb_entity_find(&e, qb_element_getid(element[0]));
          //qb_entity_destroy(&e);
          t.p += t.v;
          
          qb_element_write(element[0]);
        });
  	qb_system_create(&benchmark, attr);
  	qb_systemattr_destroy(&attr);
  }

  std::cout << "Starting engine\n";
  qb_start();
  
  std::cout << "Creating entities\n";
  for (uint64_t i = 0; i < count; ++i) {
    qbEntityAttr attr;
      qb_entityattr_create(&attr);
    glm::vec3 p{(float)i, 0, 0};
    glm::vec3 v{1, 0, 0};
    qbEntity entity;
    Transformation t{p, v};
    qb_entityattr_addcomponent(attr, transformations, &t);
    qb_entity_create(&entity, attr);
    //qb_entity_addcomponent(entity, transformations, &t);
    //qb_entity_removecomponent(entity, transformations);
    //qb_entity_addcomponent(entity, transformations, &t);
    qb_entityattr_destroy(&attr);
  }
  std::cout << "Done creating entities\n";
  
  double total = 0.0;
  double avg = 0.0;
  Timer timer;
  std::cout << "looping\n";
  for (uint64_t i = 0; i < iterations; ++i) {
    timer.start();
    qb_loop();
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
  qb_stop();

  while (1);

  return 0;
}
#endif