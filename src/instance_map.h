#ifndef INSTANCE_MAP__H
#define INSTANCE_MAP__H

#include <cubez/common.h>
#include "block_vector.h"

#include <vector>

#include "utils.h"

class InstanceMap {
public:
  typedef uint64_t Key;

  class iterator {
  public:
    iterator() = default;

    iterator operator++() {
      ++index_;
      return *this;
    }

    iterator operator+(size_t delta) const {
      return iterator(map_, std::min(map_->size(), index_ + delta));
    }

    iterator operator+=(size_t delta) {
      index_ = std::min(map_->size(), index_ + delta);
      return *this;
    }

    bool operator==(const iterator& other) const {
      return map_ == other.map_ && index_ == other.index_;
    }

    bool operator!=(const iterator& other) const {
      return !(*this == other);
    }

    std::pair<qbId, void*> operator*() {
      return{ map_->dense_[index_], map_->dense_values_[index_] };
    }

  private:
    iterator(InstanceMap* map, size_t index) : map_(map), index_(index) {}

    InstanceMap* map_ = nullptr;
    size_t index_ = 0;

    friend class InstanceMap;
  };

  class const_iterator {
  public:
    const_iterator operator++() {
      ++index_;
      return *this;
    }

    const_iterator operator+(size_t delta) const {
      return const_iterator(map_, std::min(map_.size(), index_ + delta));
    }

    const_iterator operator+=(size_t delta) {
      index_ = std::min(map_.size(), index_ + delta);
      return *this;
    }

    bool operator==(const const_iterator& other) const {
      return &map_ == &other.map_ && index_ == other.index_;
    }

    bool operator!=(const const_iterator& other) const {
      return !(*this == other);
    }

    std::pair<qbId, const void*> operator*() const {
      return{ map_.dense_[index_], map_.dense_values_[index_] };
    }

  private:
    const_iterator(const InstanceMap& map, size_t index) : map_(map), index_(index) {}

    const InstanceMap& map_;
    size_t index_;

    friend class InstanceMap;
  };

  InstanceMap(size_t element_size);
  InstanceMap(const InstanceMap& other);
  InstanceMap(InstanceMap&& other);

  InstanceMap& operator=(const InstanceMap& other);
  InstanceMap& operator=(InstanceMap&& other);

  void reserve(size_t size);

  void* operator[](uint64_t key);
  const void* operator[](uint64_t key) const;
  void* at(uint64_t key);

  iterator begin();
  iterator end();
  const_iterator begin() const;
  const_iterator end() const;

  void insert(uint64_t key, void* value);
  void erase(uint64_t key);
  void clear();
  bool has(uint64_t key) const;

  uint64_t size() const;
  size_t capacity() const;
  size_t element_size() const;

private:
  void copy(const InstanceMap& other);
  void move(const InstanceMap& other);

  size_t element_size_;
  std::vector<qbId> sparse_;
  BlockVector dense_values_;
  std::vector<uint64_t> dense_;
};


#endif  // INSTANCE_MAP__H