#include <cubez/cubez.h>
#include <cubez/utils.h>

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
  qb_instance_mutable(insts[0], &p);
  qb_instance_const(insts[1], &d);

  float dt = *(float*)(f->state);
  p->p += d->dir * dt;
}

void comflab(qbInstance* insts, qbFrame*) {
  ComflabulationComponent* comflab;
  qb_instance_mutable(insts[0], &comflab);
  comflab->thingy *= 1.000001f;
  comflab->mingy = !comflab->mingy;
  comflab->dingy++;
}

double create_entities_benchmark(uint64_t count, uint64_t /** iterations */) {
  qbTimer timer;
  qb_timer_create(&timer, 0);

  qb_timer_start(timer);
  qbEntityAttr attr;
  qb_entityattr_create(&attr);
  for (uint64_t i = 0; i < count; ++i) {    
    qbEntity entity;
    qb_entity_create(&entity, attr);
  }
  qb_loop(0, 0);
  qb_entityattr_destroy(&attr);
  qb_timer_stop(timer);

  
  double result = qb_timer_elapsed(timer);
  qb_timer_destroy(&timer);

  return result;
}

int64_t count = 0;
int64_t* Count() {
  return &count;
}

double iterate_unpack_one_component_benchmark(uint64_t count, uint64_t iterations) {
  qbTimer timer;
  qb_timer_create(&timer, 0);
  {
    qbEntityAttr attr;
    qb_entityattr_create(&attr);
    PositionComponent p;
    qb_entityattr_addcomponent(attr, position_component, &p);

    for (uint64_t i = 0; i < count; ++i) {
      qbEntity entity;
      p.p.x++;
      qb_entity_create(&entity, attr);
    }

    qb_entityattr_destroy(&attr);
  }
  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_addconst(attr, position_component);
    qb_systemattr_setfunction(attr,
      [](qbInstance* instance, qbFrame*) {        

        PositionComponent p;
        qb_instance_const(*instance, &p);
        *Count() += (uint64_t)p.p.x ^ 0x12983;
      });

    qbSystem system;
    qb_system_create(&system, attr);
    qb_systemattr_destroy(&attr);
  }
  qbQueryComponent_ all[] = { {position_component} };

  qbQuery_ query
  {
    [](qbEntity entity, qbVar, qbVar) {
      PositionComponent p;
      qb_instance_find(position_component, entity, &p);
      *Count() += (uint64_t)p.p.x ^ 0x12983;
      return QB_QUERY_RESULT_CONTINUE;
    },

    sizeof(all) / sizeof(all[0]),
    all
  };

  qb_loop(0, 0);
  qb_timer_start(timer);
  for (uint64_t i = 0; i < iterations; ++i) {
    qb_loop(0, 0);
    //qb_query(&query, qbNil);
  }
  qb_timer_stop(timer);
  std::cout << "Count = " << *Count() << std::endl;

  double elapsed = qb_timer_elapsed(timer);
  qb_timer_destroy(&timer);
  return elapsed;
}

double coroutine_overhead_benchmark(uint64_t count, uint64_t iterations) {
  qbTimer timer;
  qb_timer_create(&timer, 0);

  *Count() = 0;
  for (auto i = 0; i < count; ++i) {
    qb_coro_sync([](qbVar) {
      for (;;) {
        *Count() += 1;
        qb_coro_yield(qbInt(0));
      }
      return qbInt(0);
    }, qbInt(0));
  }

  qb_timer_start(timer);
  qb_loop(0, 0);
  for (uint64_t i = 0; i < iterations; ++i) {
    qb_loop(0, 0);
  }
  qb_timer_stop(timer);

  std::cout << "Count = " << *Count() << std::endl;

  double elapsed = qb_timer_elapsed(timer);
  qb_timer_destroy(&timer);
  return elapsed;
}

template<class F>
void do_benchmark(const char* name, F f, uint64_t count, uint64_t iterations, uint64_t test_iterations) {
  std::cout << "Running benchmark: " << name << "\n";
  uint64_t elapsed = 0;
  for (uint64_t i = 0; i < test_iterations; ++i) {
    uint64_t before = elapsed;
    qbScene test_scene;
    qb_scene_create(&test_scene, "");
    qb_scene_activate(test_scene);

    elapsed += f(count, iterations) / iterations;

    qb_scene_destroy(&test_scene);
    if (before > elapsed) {
      std::cout << "overflow\n";
    }
  }
  std::cout << "Finished benchmark\n";
  std::cout << "Total elapsed: " << elapsed << "ns\n";
  std::cout << "Elapsed per iteration: " << elapsed / test_iterations << "ns\n";
  std::cout << "Total elapsed: " << (double)elapsed / 1e9 << "s\n";
  std::cout << "Elapsed per iteration: " << ((double)elapsed / 1e9) / test_iterations << "s\n";
  if (count) {
    std::cout << "Elapsed per iteration per obj: " << ((double)elapsed) / test_iterations / count << "ns\n";
  }
}

qbSchema pos_schema;
qbSchema vel_schema;
qbComponent pos_component;
qbComponent vel_component;

int main() {
  std::cout << "Number of processors: " << omp_get_max_threads() << std::endl;
  omp_set_num_threads(omp_get_max_threads());
  //omp_set_num_threads(1);

  qbUniverse uni;
  qbUniverseAttr_ attr = {};
  attr.enabled = (qbFeature)(QB_FEATURE_GAME_LOOP | QB_FEATURE_LOGGER);
  
  qbScriptAttr_ script_attr = {};
  attr.script_args = &script_attr;

  qb_init(&uni, &attr);
  qb_start();

  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, PositionComponent);

    qb_component_create(&position_component, "Pos", attr);
    qb_componentattr_destroy(&attr);
  }
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, DirectionComponent);
    qb_component_create(&direction_component, "Direction", attr);
    qb_componentattr_destroy(&attr);
  }
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, ComflabulationComponent);
    qb_component_create(&comflabulation_component, "Comfabulation", attr);
    qb_componentattr_destroy(&attr);
  }
  uint64_t count = 1000000;
  uint64_t iterations = 500;
  uint64_t test_iterations = 1;
#if 0
  pos_component = qb_component_find("Position", &pos_schema);
  vel_component = qb_component_find("Velocity", &vel_schema);

  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_addconst(attr, pos_component);
    qb_systemattr_addconst(attr, vel_component);
    qb_systemattr_setfunction(attr, [](qbInstance* insts, qbFrame*) {
      std::pair<double, double>* p;
      qb_instance_const(insts[0], &p);

      std::pair<double, double>* v;
      qb_instance_const(insts[1], &v);
      
      p->first += v->first;
      p->second += v->second;
    });
    qbSystem s;
    qb_system_create(&s, attr);

    qb_systemattr_destroy(&attr);
  }




  
  qbTimer timer;
  qb_timer_create(&timer, 0);
  
  qb_loop(0, 0);
  double elapsed = 0.0;
  for (uint64_t i = 0; i < iterations; ++i) {
    qb_timer_start(timer);
    qb_loop(0, 0);
    qb_timer_stop(timer);
    double e = qb_timer_elapsed(timer);
    elapsed += e;
    std::cout << i << "] " << e / 1e9 << "s, " << elapsed / i / count << ", " << e / count << std::endl;
  }
  
  std::cout << "Total elapsed: " << (double)elapsed / 1e9 << "s\n";
  std::cout << "Elapsed per iteration: " << ((double)elapsed / 1e9) / iterations << "s\n";
  std::cout << "Elapsed per object: " << ((double)elapsed / 1e9) / iterations / count << "s\n";
  std::cout << "Elapsed per object: " << ((double)elapsed) / iterations / count << "ns\n";
  qb_timer_destroy(&timer);
#endif  
  //do_benchmark("Create entities benchmark",
  //             create_entities_benchmark, count, iterations, 1);
  do_benchmark("Unpack one component benchmark",
    iterate_unpack_one_component_benchmark, count, iterations, test_iterations);
  /*do_benchmark("coroutine_overhead_benchmark",
               coroutine_overhead_benchmark, 1, 1000000, 1);*/
  qb_stop();
  while (1);
}

/*
// Hypothesis: accessing the _fields table is slowing down due to hashmap lookup and GC.
Total elapsed: 11.992s
Elapsed per iteration: 1.1992s
Elapsed per object: 1.1992e-05s
Elapsed per object: 11992ns

// 

*/

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