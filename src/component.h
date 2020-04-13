/**
* Author: Samuel Rohde (rohde.samuel@cubez.io)
*
* Copyright 2020 Samuel Rohde
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#ifndef COMPONENT__H
#define COMPONENT__H

#include <cubez/cubez.h>
#include "sparse_map.h"
#include "sparse_set.h"

#include <shared_mutex>

// Not thread-safe. 
class Component {
  typedef SparseMap<void, BlockVector> InstanceMap;
 public:
  typedef typename InstanceMap::iterator iterator;
  typedef typename InstanceMap::const_iterator const_iterator;

  Component(qbId id, size_t instance_size, bool is_shared, qbComponentType type);

  Component* Clone();
  void Merge(const Component& other);

  qbResult Create(qbId entity, void* value);
  qbResult Destroy(qbId entity);

  void* operator[](qbId entity);
  const void* operator[](qbId entity) const;
  const void* at(qbId entity) const;

  bool Has(qbId entity) const;

  bool Empty() const;
  size_t Size() const;
  void Reserve(size_t count);

  size_t ElementSize() const;
  qbId Id() const;

  void Lock(bool is_mutable=false);
  void Unlock(bool is_mutable = false);

  iterator begin();
  iterator end();

  const_iterator begin() const;
  const_iterator end() const;

 private:
  qbId id_;
  InstanceMap instances_;

  std::shared_mutex mu_;
  const bool is_shared_;
  qbComponentType type_;
};

#endif
