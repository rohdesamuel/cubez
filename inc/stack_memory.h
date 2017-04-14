/**
* Author: Samuel Rohde (rohde.samuel@gmail.com)
*
* This file is subject to the terms and conditions defined in
* file 'LICENSE.txt', which is part of this source code package.
*/

#ifndef STACK_MEMORY__H
#define STACK_MEMORY__H

#include "common.h"
#include <memory.h>

static const int MAX_SIZE = 512;

namespace cubez
{

class Stack {
private:
  struct StackFrame {
    size_t size;
  };

  bool should_delete_ = false;
  uint8_t* stack_;
  // Always points to the next empty piece of memory.
  uint8_t* top_;
  uint8_t* ceiling_;

public:
  Stack(uint8_t* stack, size_t size) : stack_(stack), top_(stack_), ceiling_(stack_ + size) {}
  Stack(size_t size = MAX_SIZE) : Stack(new uint8_t[size], size) { should_delete_ = true; }
  ~Stack() {
    clear();
    if (should_delete_ && stack_) {
      delete[] stack_;
    }
  }

  void* alloc(size_t type_size) {
    void* ret = nullptr;

    StackFrame frame{type_size};
    uint8_t* new_top = top_ + type_size + sizeof(StackFrame);
    if (new_top <= ceiling_) {
      *(StackFrame*)(new_top - sizeof(StackFrame)) = frame;
      ret = top_;
      top_ = new_top;
    }

    DEBUG_ASSERT(ret, Status::Code::MEMORY_OUT_OF_BOUNDS);
    return ret;
  }

  // Return pointer to value that is on the top of the stack.
  void* top() const {
    void* ret = nullptr;

    StackFrame* frame = (StackFrame*)(top_ - sizeof(StackFrame));
    if ((uint8_t*)(frame) >= stack_) {
      ret = top_ - sizeof(StackFrame) - frame->size;
    }

    return ret;
  }

  void free() {
    StackFrame* frame = (StackFrame*)(top_ - sizeof(StackFrame));
    if ((uint8_t*)(frame) >= stack_) {
      top_ = top_ - sizeof(StackFrame) - frame->size;
    }
  }

  void clear() {
    top_ = stack_;
  }
};

}  // namespace cubez
#endif
