#ifndef FRAME__H
#define FRAME__H

#include <functional>
#include <iostream>

#include "common.h"
#include "stack_memory.h"

namespace radiance
{

class Frame {
private:
  Stack stack_;
  void* ret_ = nullptr;

public:
  template<typename Type_>
  Type_* push(const Type_& t) {
    Type_* new_t = (Type_*)stack_.alloc(sizeof(Type_));
    new (new_t) Type_(t);
    return new_t;
  }

  template<typename Type_>
  Type_* push(Type_&& t) {
    Type_* new_t = (Type_*)stack_.alloc(sizeof(Type_));
    new (new_t) Type_(std::move(t));
    return new_t;
  }

  void pop() {
    stack_.free();
  }

  template<typename Type_>
  inline Type_* peek() const {
    return (Type_*)stack_.top();
  }

  void clear() {
    stack_.clear();
  }
};

}  // namespace radiance

#endif  // FRAME__H
