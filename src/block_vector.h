#ifndef BLOCK_VECTOR__H
#define BLOCK_VECTOR__H

#include "apex_memmove.h"
#include <cubez/cubez.h>

#include <algorithm>
#include <cstring>
#include <vector>

#ifdef __COMPILE_AS_WINDOWS__
#include <malloc.h>
#else
#include <stdlib.h>
#endif

class BlockVector {
public:
  class iterator {
  public:
    iterator(BlockVector* vector, size_t index)
        : vector_(vector), index_(index) {}

    void* operator*() {
      return (*vector_)[index_];
    }

    iterator operator++() {
      ++index_;
      return *this;
    }

    bool operator==(const iterator& other) {
      return index_ == other.index_;
    }

    bool operator!=(const iterator& other) {
      return !(*this == other);
    }

  private:
    BlockVector* vector_;
    size_t index_;
  };
  typedef const iterator const_iterator;
  typedef uint64_t Index;

  BlockVector() : count_(0), capacity_(0), elem_size_(0) {}

  BlockVector(size_t element_size) :
    count_(0), capacity_(0), elem_size_(element_size) {
    elems_.push_back(alloc_block());
    capacity_ = elem_size_ == 0 ? 0 : page_size_ / elem_size_;
    size_t initial_capacity = 8;
    reserve(initial_capacity);
  }

  BlockVector(const BlockVector& other)
      : elem_size_(other.elem_size_), page_size_(other.page_size_) {
    copy(other);
  }

  BlockVector(BlockVector&& other)
      : elem_size_(other.elem_size_), page_size_(other.page_size_) {
    move(std::move(other));
  }

  ~BlockVector() {
    for (void* e : elems_) {
      ALIGNED_FREE(e);
    }
  }

  BlockVector& operator=(const BlockVector& other) {
    if (this != &other) {
      copy(other);
    }
    return *this;
  }

  BlockVector& operator=(BlockVector&& other) {
    if (this != &other) {
      move(std::move(other));
    }
    return *this;
  }

  void* operator[](Index index) {
    size_t block = (index * elem_size_) / (page_size_ - elem_size_);
    size_t block_index = (index * elem_size_) % (page_size_ - elem_size_);
    return (uint8_t*)(elems_[block]) + block_index;
  }

  const void* operator[](Index index) const {
    size_t block = (index * elem_size_) / (page_size_ - elem_size_);
    size_t block_index = (index * elem_size_) % (page_size_ - elem_size_);
    return (uint8_t*)(elems_[block]) + block_index;
  }

  iterator begin() {
    return iterator(this, 0);
  }

  iterator end() {
    return iterator(this, count_);
  }

  // Returns the addres to the first element.
  void* front() {
    return elems_[0];
  }

  // Returns the address to the last element.
  void* back() {
    return (*this)[count_ - 1];
  }

  // Returns the addres to the first element.
  const void* front() const {
    return elems_[0];
  }

  // Returns the address to the last element.
  const void* back() const {
    return (*this)[count_ - 1];
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
    ++count_;
    resize_capacity(count_ + 1);
    if (data) {
      apex::memmove((*this)[count_ - 1], data, elem_size_);
    }
  }

  void push_back(const void* data) {
    ++count_;
    if (count_ >= capacity_) {
      resize_capacity(count_ + 1);
    }
    if (data) {
      apex::memcpy((*this)[count_ - 1], data, elem_size_);
    }
  }

  void pop_back() {
    if (count_ == 0) {
      return;
    }

    --count_;
    if (count_ < capacity_) {
      resize_capacity(count_);
    }
  }

private:
  void resize_capacity(size_t count) {
    if (count <= 8) {
      count = 8;
    }

    if ((count * elem_size_) > elems_.size() * (page_size_ - elem_size_) && elem_size_ > 0) {
      while (elems_.size() <= (count * elem_size_) / (page_size_ - elem_size_)) {
        capacity_ += page_size_ / elem_size_;
        elems_.push_back(alloc_block());
      }
    }/* else if (count < capacity_ / 4 && elems_.size() > 1) {
      capacity_ = std::max(capacity_ - (page_size_ / elem_size_), size_t(0));
      free(elems_.back());
      elems_.pop_back();
    }*/
  }

  void* alloc_block() {
    return ALIGNED_ALLOC(page_size_ + elem_size_, page_size_);
  }

  void copy(const BlockVector& other) {
    count_ = other.count_;
    capacity_ = other.capacity_;
    reserve(count_);
    *(size_t*)(&elem_size_) = other.elem_size_;
    *(size_t*)(&page_size_) = other.page_size_;
    for (void* e : other.elems_) {
      void* copy = alloc_block();
      apex::memmove(copy, e, page_size_);
      elems_.push_back(copy);
    }
  }

  void move(BlockVector&& other) {
    count_ = other.count_;
    capacity_ = other.capacity_;
    elems_ = other.elems_;
    *(size_t*)(&elem_size_) = other.elem_size_;

    other.count_ = 0;
    other.capacity_ = 0;
    other.elems_.clear();
  }

  std::vector<void*> elems_;
  size_t count_;
  size_t capacity_;
  const size_t elem_size_;
  const size_t page_size_ = 4096;
};

template<class Ty_>
class TypedBlockVector {
public:
  class iterator {
  public:
    iterator(TypedBlockVector* vector, size_t index)
      : vector_(vector), index_(index) {}

    Ty_& operator*() {
      return (*vector_)[index_];
    }

    iterator operator++() {
      ++index_;
      return *this;
    }

    bool operator==(const iterator& other) {
      return index_ == other.index_;
    }

    bool operator!=(const iterator& other) {
      return !(*this == other);
    }

  private:
    TypedBlockVector* vector_;
    size_t index_;
  };
  class const_iterator {
  public:
    const_iterator(TypedBlockVector* vector, size_t index)
      : vector_(vector), index_(index) {}

    const Ty_& operator*() {
      return (*vector_)[index_];
    }

    iterator operator++() {
      ++index_;
      return *this;
    }

    bool operator==(const iterator& other) {
      return index_ == other.index_;
    }

    bool operator!=(const iterator& other) {
      return !(*this == other);
    }

  private:
    TypedBlockVector* vector_;
    size_t index_;
  };

  typedef BlockVector::Index Index;

  TypedBlockVector() : elems_(sizeof(Ty_)) {
    elems_.reserve(8);
  }

  TypedBlockVector(const TypedBlockVector& other) {
    copy(other);
  }

  TypedBlockVector(TypedBlockVector&& other) {
    move(std::move(other));
  }

  TypedBlockVector& operator=(const TypedBlockVector& other) {
    elems_ = other.elems_;
  }

  TypedBlockVector& operator=(TypedBlockVector&& other) {
    elems_ = std::move(other.elems_);
  }

  Ty_& operator[](Index index) {
    return *(Ty_*)elems_[index];
  }

  const Ty_& operator[](Index index) const {
    return *(Ty_*)elems_[index];
  }

  iterator begin() {
    return iterator(this, 0);
  }

  iterator end() {
    return iterator(this, elems_.size());
  }

  const_iterator begin() const {
    return const_iterator(this, 0);
  }

  const_iterator end() const {
    return const_iterator(this, elems_.size());
  }

  // Returns the addres to the first element.
  Ty_& front() {
    return *(Ty_*)elems_.front();
  }

  // Returns the address to the last element.
  Ty_& back() {
    return *(Ty_*)elems_.back();
  }

  // Returns the addres to the first element.
  const Ty_& front() const {
    return *(Ty_*)elems_.front();
  }

  // Returns the address to the last element.
  const Ty_& back() const {
    return *(Ty_*)elems_.back();
  }

  Ty_& at(Index index) {
    return *(Ty_*)elems_.at(index);
  }

  const Ty_& at(Index index) const {
    return *(Ty_*)elems_.at(index);
  }

  bool empty() const {
    return elems_.empty();
  }

  size_t size() const {
    return elems_.size();
  }

  size_t element_size() const {
    return elems_.element_size();
  }

  void reserve(size_t count) {
    elems_.reserve(count);
  }

  void clear() {
    elems_.clear();
  }

  void resize(size_t count) {
    size_t old_size = size();
    elems_.resize(count);
    for (size_t i = old_size; i < count; ++i) {
      new ((Ty_*)elems_[i]) Ty_();
    }
  }

  size_t capacity() const {
    return elems_.capacity();
  }

  void push_back(Ty_&& data) {
    elems_.push_back((void*)nullptr);
    *(Ty_*)elems_.back() = std::move(data);
  }

  void push_back(const Ty_& data) {
    elems_.push_back((void*)nullptr);
    *(Ty_*)elems_.back() = data;
  }

  void pop_back() {
    elems_.pop_back();
  }

private:
  BlockVector elems_;
};

#endif  // BLOCK_VECTOR__H
