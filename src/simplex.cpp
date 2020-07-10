#include "simplex.h"

#include <algorithm>

size_t Simplex::Size() const {
  return points_.size();
}

void Simplex::Add(vec3s&& p) {
  points_.push_back(std::move(p));
  std::swap(points_.front(), points_.back());
}

void Simplex::Remove(vec3s&& p) {
  auto it = std::find_if(points_.begin(), points_.end(),
                         [&p](const vec3s& point) { return glms_vec3_eqv(p, point); });
  std::swap(*it, points_.back());
  points_.pop_back();
}

vec3s& Simplex::operator[](size_t i) {
  return points_[i];
}

const vec3s& Simplex::operator[](size_t i) const {
  return points_[i];
}