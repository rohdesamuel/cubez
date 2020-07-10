#ifndef SIMPLEX__H
#define SIMPLEX__H

#include <cglm/struct/vec3.h>
#include <vector>

class Simplex {
public:
  Simplex() {}
  size_t Size() const;
  void Add(vec3s&& p);
  void Remove(vec3s&& p);

  vec3s Last() {
    return points_.front();
  };

  vec3s& operator[](size_t i);
  const vec3s& operator[](size_t i) const;

private:
  std::vector<vec3s> points_;
};

#endif  // SIMPLEX__H