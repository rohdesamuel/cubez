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

namespace cubez
{

template<typename Key_, typename Value_,
         typename Allocator_ = std::allocator<Value_>>
struct Schema {
  typedef cubez::Table<Key_, Value_, Allocator_> Table;
  typedef cubez::View<Table> View;
  typedef typename Table::Element Element;
  typedef cubez::MutationBuffer<Table> MutationBuffer;
  typedef Key_ Key;
  typedef Value_ Value;
};

}  // namespace cubez

#endif
