/**
* Author: Samuel Rohde (rohde.samuel@cubez.io)
*
* Copyright {2020} {Samuel Rohde}
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

#ifndef BYTE_QUEUE__H
#define BYTE_QUEUE__H

#include "byte_vector.h"

class ByteQueue {
public:
  ByteQueue(size_t element_size)
    : ring_(element_size), overflow_(element_size),
    begin_(0), end_(0), length_(0), overflow_begin_(0),
    overflow_length_(0) {
    capacity_ = ring_.capacity();
  }
  ByteQueue(const ByteQueue& other) = default;
  ByteQueue(ByteQueue&& other) = default;

  // Returns the addres to the first element.
  void* front() {
    if (length_ > 0) {
      return ring_[begin_];
    } else if (overflow_length_ > 0) {
      return overflow_[overflow_begin_];
    }
    return nullptr;
  }

  // Returns the address to the last element.
  void* back() {
    if (overflow_length_ == 0) {
      return ring_[(end_ - 1) % capacity_];
    }
    return overflow_[overflow_length_ - 1];
  }

  // Returns the addres to the first element.
  const void* front() const {
    return  ring_[begin_];
  }

  // Returns the address to the last element.
  const void* back() const {
    if (overflow_length_ == 0) {
      return ring_[(end_ - 1) % capacity_];
    }
    return overflow_[overflow_length_ - 1];
  }

  bool empty() const {
    return length_ == 0 && overflow_length_ == 0;
  }

  size_t size() const {
    return length_ + overflow_length_;
  }

  size_t element_size() const {
    return ring_.element_size();
  }

  void reserve(size_t count) {
    ring_.reserve(count);
  }

  void resize(size_t count) {
    ring_.resize(count);
  }

  size_t capacity() const {
    return ring_.capacity();
  }

  void clear() {
    ring_.clear();
  }

  void push(void* data) {
    if (length_ < capacity_ && overflow_length_ == 0) {
      std::memmove(ring_[end_], data, ring_.element_size());
      end_ = (end_ + 1) % capacity_;
      ++length_;
    } else {
      ++overflow_length_;
      overflow_.push_back(data);

      // Push a null value to ready the ring_ for when the overflow clears.
      data = nullptr;
      ring_.push_back(data);
    }
  }

  void push(const void* data) {
    if (length_ < capacity_ && overflow_length_ == 0) {
      std::memmove(ring_[end_], data, ring_.element_size());
      end_ = (end_ + 1) % capacity_;
      ++length_;
    } else {
      ++overflow_length_;
      overflow_.push_back(data);

      // Push a null value to ready the ring_ for when the overflow clears.
      data = nullptr;
      ring_.push_back(data);
    }
  }

  void pop() {
    if (length_ > 0) {
      begin_ = (begin_ + 1) % capacity_;
      --length_;
    } else if (overflow_length_ > 0) {
      ++overflow_begin_;
      --overflow_length_;
    }

    if (overflow_length_ == 0) {
      overflow_begin_ = 0;
      overflow_.clear();

      // Reset the capacity. The ring buffer has expanded to the size of the
      // overflow.
      capacity_ = ring_.capacity();
    }
  }

private:
  bool is_in_overflow() {
    return length_ >= ring_.capacity();
  }

  ByteVector ring_;
  ByteVector overflow_;

  ByteVector::Index begin_;
  ByteVector::Index end_;
  uint64_t length_;
  uint64_t capacity_;

  ByteVector::Index overflow_begin_;
  uint64_t overflow_length_;
};

#endif  // BYTE_QUEUE__H
