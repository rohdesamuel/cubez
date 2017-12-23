/**
* Author: Samuel Rohde (rohde.samuel@gmail.com)
*
* This file is subject to the terms and conditions defined in
* file 'LICENSE.txt', which is part of this source code package.
*/

#ifndef SPARSE_SET__H
#define SPARSE_SET__H

#include <vector>

#include "common.h"

class SparseSet {
public:
  SparseSet() : sparse_(16, -1) {
    dense_.reserve(16);
  }

  void reserve(size_t size) {
    sparse_.reserve(size);
    dense_.reserve(size);
  }

  void insert(int64_t value) {
    if (value >= sparse_.size()) {
      sparse_.resize(value + 1, -1);
    }
    sparse_[value] = dense_.size();
    dense_.push_back(value);
  }

  void erase(int64_t value) {
    dense_[sparse_[value]] = dense_.back();
    sparse_[dense_.back()] = sparse_[value];
    dense_.pop_back();
    sparse_[value] = -1;
  }

  bool has(int64_t value) {
    if (value > sparse_.size()) {
      return false;
    }
    return sparse_[value] != -1;
  }

  uint64_t size() const {
    return dense_.size();
  }

private:
  std::vector<qbHandle> sparse_;
  std::vector<int64_t> dense_;
};

#endif  // SPARSE_SET__H
