#ifndef BYTE_VECTOR__H
#define BYTE_VECTOR__H

#include "common.h"
#include <algorithm>
#include <cstring>
#include <string>
#include <iostream>

class ByteVector {
 public:
  class iterator {
   public:
    iterator(uint8_t* first_element, size_t stride) :
        pos_(first_element), stride_(stride) {}
    void* operator*() {
      return pos_;
    }

    iterator operator++() {
      pos_ += stride_;
      return *this;
    }

    bool operator==(const iterator& other) {
      return pos_ == other.pos_;
    }

    bool operator!=(const iterator& other) {
      return !(*this == other);
    }

   private:
    uint8_t* pos_;
    size_t stride_;
  };
  typedef const iterator const_iterator;
  typedef uint64_t Index;

  ByteVector() : elems_(nullptr), count_(0), capacity_(0), elem_size_(0) { }

  ByteVector(size_t element_size) :
      elems_(nullptr), count_(0), capacity_(0), elem_size_(element_size) {
    size_t initial_capacity = 8;
    elems_ = (uint8_t*)calloc(initial_capacity, elem_size_);
    reserve(initial_capacity);
  }

  ByteVector(const ByteVector& other) : elem_size_(other.elem_size_) {
    copy(other);
  }

  ByteVector(ByteVector&& other) : elem_size_(other.elem_size_) {
    move(std::move(other));
  }

  ~ByteVector() {
    free(elems_);
  }

  ByteVector& operator=(const ByteVector& other) {
    if (this != &other) {
      copy(other);
    }
    return *this;
  }

  ByteVector& operator=(ByteVector&& other) {
    if (this != &other) {
      move(std::move(other));
    }
    return *this;
  }

  void* operator[](Index index) {
    return elems_ + elem_size_ * index;
  }

  const void* operator[](Index index) const {
    return elems_ + elem_size_ * index;
  }

  iterator begin() {
    return iterator(elems_, elem_size_);
  }

  iterator end() {
    return iterator(elems_ + count_ * elem_size_, elem_size_);
  }

  const_iterator begin() const {
    return iterator((uint8_t*)front(), elem_size_);
  }

  const_iterator end() const {
    return iterator((uint8_t*)(elems_ + count_ * elem_size_), elem_size_);
  }

  // Returns the addres to the first element.
  void* front() {
    return elems_;
  }

  // Returns the address to the last element.
  void* back() {
    return elems_ + (count_ - 1) * elem_size_;
  }

  // Returns the addres to the first element.
  const void* front() const {
    return elems_;
  }

  // Returns the address to the last element.
  const void* back() const {
    return elems_ + (count_ - 1) * elem_size_;
  }

  void* at(Index index) {
    if (index < count_) {
      return (*this)[index];
    }
    return nullptr;
  }

  const void* at(Index index) const {
    if (index < count_) {
      return (*this)[index];
    }
    return nullptr;
  }

  uint8_t* data() const {
    return elems_;
  }

  bool empty() const {
    return count_ == 0;
  }

  size_t size() const {
    return count_;
  }

  size_t element_size() const {
    return elem_size_;
  }

  void reserve(size_t count) {
    if (count > capacity_) {
      resize_capacity(count);
    }
  }

  void clear() {
    count_ = 0;
    resize_capacity(0);
  }

  void resize(size_t count) {
    count_ = count;
    resize_capacity(count);
  }

  size_t capacity() const {
    return capacity_;
  }

  void push_back(void* data) {
    if (count_ + 1 >= capacity_) {
      resize_capacity(capacity_ + 1);
    }
    if (data) {
      std::memmove((*this)[count_], data, elem_size_);
    }
    ++count_;
  }

  void push_back(const void* data) {
    if (count_ + 1 >= capacity_) {
      resize_capacity(capacity_ + 1);
    }
    if (data) {
      std::memcpy((*this)[count_], data, elem_size_);
    }
    ++count_;
  }

  void pop_back() {
    --count_;
    if (count_ < capacity_) {
      resize_capacity(capacity_ - 1);
    }
  }

 private:
  void resize_capacity(size_t count) {
    if (count <= 8) {
      count = 8;
    }
    if (!elems_) {
      capacity_ = count;
      elems_ = (uint8_t*)calloc(capacity_, elem_size_);
    }
    size_t log_2_count = count;
    size_t log_2 = 0;
    for (size_t i = 0; i < 8 * sizeof(size_t); ++i) {
      log_2_count = log_2_count & ~(1 << i);
      if (log_2_count == 0) {
        log_2 = i;
        break;
      }
    }

    size_t new_capacity = capacity_;
    if (count > capacity_) {
      new_capacity = (size_t)1 << (log_2 + 1);
    } else if (count < capacity_ / 4) {
      new_capacity = std::max(8, 1 << (log_2 - 1));
    }

    if (capacity_ == new_capacity) {
      return;
    }
    elems_ = (uint8_t*)realloc(elems_, new_capacity * elem_size_);
    capacity_ = new_capacity;
  }

  void copy(const ByteVector& other) {
    count_ = other.count_;
    reserve(count_);
    memcpy(elems_, other.elems_, other.size() * other.element_size());
    *(size_t*)(&elem_size_) = other.elem_size_;
  }

  void move(ByteVector&& other) {
    count_ = other.count_;
    capacity_ = other.capacity_;
    elems_ = other.elems_;
    *(size_t*)(&elem_size_) = other.elem_size_;

    other.count_ = 0;
    other.capacity_ = 0;
    other.elems_ = nullptr;
  }

  uint8_t* elems_;
  size_t count_;
  size_t capacity_;
  const size_t elem_size_;
};

#endif  // BYTE_VECTOR__H

