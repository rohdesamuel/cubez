#ifndef GJK_ALGORITHM
#define GJK_ALGORITHM

#include "mesh_builder.h"

namespace collision_utils {

class Gjk {
public:
  Gjk();
  ~Gjk();

  bool IsColliding(const Mesh&, const Mesh&);

private:

  glm::vec3 support(const Mesh& a, const Mesh& b, const glm::vec3&) const;

  bool ContainsOrigin(glm::vec3&);
  bool line(glm::vec3&);
  bool triangle(glm::vec3&);
  bool tetrahedron(glm::vec3&);

  bool checkTetrahedron(const glm::vec3&, const glm::vec3&, const glm::vec3&, const glm::vec3&, glm::vec3& dir);

  glm::vec3 a, b, c, d;
  int nrPointsSimplex = 0;
};

}



#endif