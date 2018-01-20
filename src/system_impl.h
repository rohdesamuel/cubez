#ifndef SYSTEM_IMPL__H
#define SYSTEM_IMPL__H

#include "defs.h"
#include "component_registry.h"
#include "frame_buffer.h"

#include <algorithm>
#include <cstring>

class SystemImpl {
 public:
  SystemImpl(const qbSystemAttr_& attr, qbSystem system);

  static SystemImpl* FromRaw(qbSystem system);
  
  void Run(void* event = nullptr);

  void* FindInstanceData(qbInstance instance, qbComponent component);
  qbInstance_ FindInstance(qbEntity entity, qbComponent component);

 private:
  void CopyToInstance(qbComponent component, qbEntity entity, qbInstance instance);

  void Run_0(qbFrame* f);
  void Run_1(qbFrame* f);
  void Run_N(qbFrame* f);

  qbSystem system_;
  ComponentRegistry* component_registry_;
  std::vector<qbComponent> components_;

  qbComponentJoin join_;
  void* user_state_;

  std::vector<qbInstance> instance_data_;
  std::vector<qbInstance_> instances_;  

  qbTransform transform_;
  qbCallback callback_;
};


#endif  // SYSTEM_IMPL__H

