#include <cubez/cubez.h>
#include <cubez/timer.h>

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

void move(qbInstance* insts, qbFrame* f) {
  PositionComponent* p;
  DirectionComponent* d;
  qb_instance_getmutable(insts[0], &p);
  qb_instance_getconst(insts[1], &d);

  float dt = *(float*)(f->state);
  p->p += d->dir * dt;
}

void comflab(qbInstance* insts, qbFrame*) {
  ComflabulationComponent* comflab;
  qb_instance_getmutable(insts[0], &comflab);
  comflab->thingy *= 1.000001f;
  comflab->mingy = !comflab->mingy;
  comflab->dingy++;
}

uint64_t create_entities_benchmark(uint64_t count, uint64_t /** iterations */) {
  Timer timer;
  qbEntityAttr attr;
  qb_entityattr_create(&attr);

  timer.start();
  for (uint64_t i = 0; i < count; ++i) {
    qbEntity entity;
    qb_entity_create(&entity, attr);
  }
  qb_loop();
  timer.stop();

  qb_entityattr_destroy(&attr);
  return timer.get_elapsed_ns();
}

int64_t count = 0;
int64_t* Count() {
  return &count; 
}

uint64_t iterate_unpack_one_component_benchmark(uint64_t count, uint64_t iterations) {
  Timer timer;
  {
    qbEntityAttr attr;
    qb_entityattr_create(&attr);
    PositionComponent p;
    qb_entityattr_addcomponent(attr, position_component, &p);

    for (uint64_t i = 0; i < count; ++i) {
      qbEntity entity;
      ++p.p.x;
      qb_entity_create(&entity, attr);
    }

    qb_entityattr_destroy(&attr);
  }
  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_addmutable(attr, position_component);
    qb_systemattr_setfunction(attr,
      [](qbInstance* instance, qbFrame*) {        

        PositionComponent p;
        qb_instance_getmutable(*instance, &p);
        *Count() += p.p.x;
      });

    qbSystem system;
    qb_system_create(&system, attr);
    qb_systemattr_destroy(&attr);
  }
  qb_loop();
  timer.start();
  for (uint64_t i = 0; i < iterations; ++i) {
    qb_loop();
  }
  timer.stop();
  std::cout << "Count = " << count << std::endl;

  return timer.get_elapsed_ns();
}

template<class F>
void do_benchmark(const char* name, F f, uint64_t count, uint64_t iterations, uint64_t test_iterations) {
  std::cout << "Running benchmark: " << name << "\n";
  uint64_t elapsed = 0;
  for (uint64_t i = 0; i < test_iterations; ++i) {
    elapsed += f(count, iterations);
  }
  std::cout << "Finished benchmark\n";
  std::cout << "Total elapsed: " << elapsed << "ns\n";
  std::cout << "Elapsed per iteration: " << elapsed / test_iterations << "ns\n";
  std::cout << "Total elapsed: " << (double)elapsed / 1e9<< "s\n";
  std::cout << "Elapsed per iteration: " << ((double)elapsed / 1e9) / test_iterations << "s\n";
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
  uint64_t test_iterations = 1;
  
  /*do_benchmark("Create entities benchmark",
               create_entities_benchmark, count, iterations, 1);*/
  do_benchmark("Unpack one component benchmark",
    iterate_unpack_one_component_benchmark, count, iterations, test_iterations);
  qb_stop();
  while (1);
}

/*
Before
#1
Total elapsed: 462616369ns
Elapsed per iteration: 462616369ns
Total elapsed: 0.462616s
Elapsed per iteration: 0.462616s

#2
Total elapsed: 480803490ns
Elapsed per iteration: 480803490ns
Total elapsed: 0.480803s
Elapsed per iteration: 0.480803s

#3
Total elapsed: 456288546ns
Elapsed per iteration: 456288546ns
Total elapsed: 0.456289s
Elapsed per iteration: 0.456289s

#4
Total elapsed: 461743162ns
Elapsed per iteration: 461743162ns
Total elapsed: 0.461743s
Elapsed per iteration: 0.461743s

#5
Total elapsed: 457279816ns
Elapsed per iteration: 457279816ns
Total elapsed: 0.45728s
Elapsed per iteration: 0.45728s


After
#1
Total elapsed: 40476674ns
Elapsed per iteration: 40476674ns
Total elapsed: 0.0404767s
Elapsed per iteration: 0.0404767s

#2
Total elapsed: 43829517ns
Elapsed per iteration: 43829517ns
Total elapsed: 0.0438295s
Elapsed per iteration: 0.0438295s

#3


#4


#5


After2
#1
Total elapsed: 66839651ns
Elapsed per iteration: 66839651ns
Total elapsed: 0.0668397s
Elapsed per iteration: 0.0668397s
*/