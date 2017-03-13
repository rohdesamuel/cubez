/**
* Author: Samuel Rohde (rohde.samuel@gmail.com)
*
* This file is subject to the terms and conditions defined in
* file 'LICENSE.txt', which is part of this source code package.
*/

#ifndef SYSTEM__H
#define SYSTEM__H

#include <functional>
#include <vector>

#include "common.h"
#include "frame.h"

namespace radiance
{

class System {
private:
  typedef std::function<void(Frame*)> Function;
  Function f_;

public:
  template<typename Function_, typename... State_>
  static System Bind(Function_ f, State_... state) {
    System s;
    s.f_ = [=](Frame* frame) {
      return f(frame, state...);
    };
    return s;
  }

  System() {
    f_ = [](Frame*){};
  }

  template<typename Function_>
  System(Function_ f) : f_(f) {}

  /*
  template<typename Function_, typename... State_>
  System(Function_ f, State_... state) {
    f_ = [=](Frame* frame) {
      return f(frame, state...);
    };
  }*/

  inline void operator()(Frame* frame) const {
    f_(frame);
  }
};

typedef std::vector<System> SystemList;

class SystemExecutor {
 public:
  struct Element {
    Id id;
    System system;
  };

  typedef std::vector<Element> Systems;
  typedef typename Systems::iterator iterator;
  typedef typename Systems::const_iterator const_iterator;

  SystemExecutor() {}
  SystemExecutor(System callback): callback_(callback) {}

  void operator()(Frame* frame) const {
    for(auto& el : systems_) {
      el.system(frame);
    }
    callback_(frame);
  }

  std::vector<Id> push(std::vector<System> systems) {
    std::vector<Id> ret;
    ret.reserve(systems.size());
    for(System& system : systems) {
      ret.push_back(push(system));
    }
    return ret;
  }

  System& callback() {
    return callback_;
  }

  void erase(iterator it) {
    systems_.erase(it);
  }

  void erase(Id id) {
    iterator it = begin();
    while(it != end()) {
      if (it->id == id) {
        break;
      }
    }
  }

  iterator begin() {
    return systems_.begin();
  }

  iterator end() {
    return systems_.end();
  }

  const_iterator begin() const {
    return systems_.begin();
  }

  const_iterator end() const {
    return systems_.end();
  }

 private:
  Id push(System system) {
    Id new_id = id_++;  
    systems_.push_back({new_id, system});
    return new_id;
  }

  Systems systems_;
  System callback_ = [](Frame*){};
  Id id_;
};

}  // namespace radiance

#endif
