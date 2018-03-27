#ifndef BYTE_VECTOR__H
#define BYTE_VECTOR__H

#include "apex_memmove.h"
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

  class const_iterator {
   public:
    const_iterator(uint8_t* first_element, size_t stride) :
        pos_(first_element), stride_(stride) {}
    const void* operator*() {
      return pos_;
    }

    const_iterator operator++() {
      pos_ += stride_;
      return *this;
    }

    bool operator==(const const_iterator& other) {
      return pos_ == other.pos_;
    }

    bool operator!=(const const_iterator& other) {
      return !(*this == other);
    }

   private:
    uint8_t* pos_;
    size_t stride_;
  };
  typedef uint64_t Index;

  ByteVector() : elems_(nullptr), count_(0), capacity_(0), elem_size_(0) { }

  ByteVector(size_t element_size) :
      elems_(nullptr), count_(0), capacity_(0), elem_size_(element_size) {
    size_t initial_capacity = 8;
    elems_ = (uint8_t*)ALIGNED_ALLOC(initial_capacity * elem_size_, 4096);
    reserve(initial_capacity);
  }

  ByteVector(const ByteVector& other) : elem_size_(other.elem_size_) {
    copy(other);
  }

  ByteVector(ByteVector&& other) : elem_size_(other.elem_size_) {
    move(std::move(other));
  }

  ~ByteVector() {
    ALIGNED_FREE(elems_);
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
    return const_iterator((uint8_t*)front(), elem_size_);
  }

  const_iterator end() const {
    return const_iterator((uint8_t*)(elems_ + count_ * elem_size_), elem_size_);
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
      apex::memmove((*this)[count_], data, elem_size_);
    }
    ++count_;
  }

  void push_back(const void* data) {
    if (count_ + 1 >= capacity_) {
      resize_capacity(capacity_ + 1);
    }
    if (data) {
      apex::memmove((*this)[count_], data, elem_size_);
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
    uint8_t* new_elems = (uint8_t*)ALIGNED_ALLOC(new_capacity * elem_size_, 4096);
    apex::memmove(new_elems, elems_, capacity_ * elem_size_);
    ALIGNED_FREE(elems_);
    elems_ = new_elems;
    capacity_ = new_capacity;
  }

  void copy(const ByteVector& other) {
    count_ = other.count_;
    reserve(count_);
    apex::memmove(elems_, other.elems_, other.size() * other.element_size());
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

template<class Ty_>
class TypedByteVector {
public:
  class iterator {
  public:
    iterator(ByteVector::iterator it) : it_(it) {}

    Ty_& operator*() {
      return *(Ty_*)(*it_);
    }

    iterator operator++() {
      ++it_;
      return *this;
    }

    bool operator==(const iterator& other) {
      return it_ == other.it_;
    }

    bool operator!=(const iterator& other) {
      return !(*this == other);
    }

  private:
    ByteVector::iterator it_;
  };
  class const_iterator {
  public:
    const_iterator(ByteVector::const_iterator it) : it_(it) {}

    const Ty_& operator*() {
      return *(Ty_*)(*it_);
    }

    const_iterator operator++() {
      ++it_;
      return *this;
    }

    bool operator==(const const_iterator& other) {
      return it_ == other.it_;
    }

    bool operator!=(const const_iterator& other) {
      return !(*this == other);
    }

  private:
    ByteVector::const_iterator it_;
  };

  typedef ByteVector::Index Index;

  TypedByteVector() : container_(sizeof(Ty_)) {}

  TypedByteVector(const TypedByteVector& other) {
    container_ = other.container_;
  }

  TypedByteVector(TypedByteVector&& other) {
    container_ = std::move(other.container_);
  }

  TypedByteVector& operator=(const TypedByteVector& other) {
    if (this != &other) {
      container_ = other.container_;
    }
    return *this;
  }

  TypedByteVector& operator=(TypedByteVector&& other) {
    if (this != &other) {
      container_ = std::move(other.container_);
    }
    return *this;
  }

  Ty_& operator[](Index index) {
    return *(Ty_*)container_[index];
  }

  const Ty_& operator[](Index index) const {
    return *(Ty_*)container_[index];
  }

  iterator begin() {
    return iterator(container_.begin());
  }

  iterator end() {
    return iterator(container_.end());
  }

  const_iterator begin() const {
    return const_iterator(container_.begin());
  }

  const_iterator end() const {
    return const_iterator(container_.end());
  }

  // Returns the addres to the first element.
  Ty_& front() {
    return *(Ty_*)container_.front();
  }

  // Returns the address to the last element.
  Ty_& back() {
    return *(Ty_*)container_.back();
  }

  // Returns the addres to the first element.
  const Ty_& front() const {
    return *(Ty_*)container_.front();
  }

  // Returns the address to the last element.
  const Ty_& back() const {
    return *(Ty_*)container_.back();
  }

  Ty_& at(Index index) {
    return (*this)[index];
  }

  const Ty_& at(Index index) const {
    return container_.at(index);
  }

  uint8_t* data() const {
    return container_.data();
  }

  bool empty() const {
    return container_.empty();
  }

  size_t size() const {
    return container_.size();
  }

  size_t element_size() const {
    return container_.element_size();
  }

  void reserve(size_t count) {
    container_.reserve(count);
  }

  void clear() {
    container_.clear();
  }

  void resize(size_t count) {
    container_.resize(count);
  }

  size_t capacity() const {
    return container_.capacity();
  }

  void push_back(Ty_&& data) {
    container_.push_back(&data);
  }

  void push_back(const Ty_& data) {
    container_.push_back(&data);
  }

  void pop_back() {
    container_.pop_back();
  }

private:
  ByteVector container_;
};

#endif  // BYTE_VECTOR__H
