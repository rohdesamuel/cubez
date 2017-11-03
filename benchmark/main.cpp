#include "inc/cubez.h"
#include "inc/timer.h"

#include <omp.h>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <glm/glm.hpp>

typedef std::vector<glm::vec3> Vectors;

struct Transformation {
  glm::vec3 p;
  glm::vec3 v;
};

int main() {

  uint64_t count = 4096;
  uint64_t iterations = 100000;
  std::cout << "Number of threads: " << omp_get_max_threads() << std::endl;
  std::cout << "Number of iterations: " << iterations << std::endl;
  std::cout << "Entity count: " << count << std::endl;

  qbUniverse uni;
  qb_init(&uni);
  
  qbComponent transformations;
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
  	qb_systemattr_setfunction(attr, +[](qbElement* element, qbCollectionInterface*, qbFrame*){
  			Transformation t;
  			qb_element_read(element[0], &t);
  			
  			t.p += t.v;
  			
  			qb_element_write(element[0]);
  		});
  	qb_system_create(&benchmark, attr);
  	qb_systemattr_destroy(&attr);
  }

  qb_start();
  
  for (uint64_t i = 0; i < count; ++i) {
	qbEntityAttr attr;
  	qb_entityattr_create(&attr);
	glm::vec3 p{(float)i, 0, 0};
	glm::vec3 v{1, 0, 0};
	Transformation t{p, v};
	qb_entityattr_addcomponent(attr, transformations, &t);
	qbEntity unused;
	qb_entity_create(&unused, attr);
	qb_entityattr_destroy(&attr);
  }
  
  double total = 0.0;
  double avg = 0.0;
  Timer timer;
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

  return 0;
}
