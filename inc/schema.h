/**
* Author: Samuel Rohde (rohde.samuel@gmail.com)
*
* This file is subject to the terms and conditions defined in
* file 'LICENSE.txt', which is part of this source code package.
*/

#ifndef SCHEMA__H
#define SCHEMA__H

#include "common.h"
#include "table.h"

namespace radiance
{

template<typename Key_, typename Value_,
         typename Allocator_ = std::allocator<Value_>>
struct Schema {
  typedef radiance::Table<Key_, Value_, Allocator_> Table;
  typedef radiance::View<Table> View;
  typedef typename Table::Element Element;
  typedef radiance::MutationBuffer<Table> MutationBuffer;
  typedef Key_ Key;
  typedef Value_ Value;
};

}  // namespace radiance

#endif
