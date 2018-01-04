#ifndef SYSTEM_IMPL__H
#define SYSTEM_IMPL__H

#include "defs.h"
#include "component_registry.h"

#include <algorithm>
#include <cstring>

class SystemImpl {
 public:
  SystemImpl(ComponentRegistry* component_registry,
             const qbSystemAttr_& attr, qbSystem system);

  static SystemImpl* FromRaw(qbSystem system);
  
  void Run(void* event = nullptr);

 private:
  void CopyToElement(qbComponent component, qbId instance_id, void* instance_data, qbElement element);

  void Run_0(qbFrame* f);
  void Run_1(qbFrame* f);
  void Run_N(qbFrame* f);

  ComponentRegistry* component_registry_;
  qbSystem system_;
  std::vector<qbComponent> sources_;
  std::vector<qbComponent> sinks_;

  qbComponentJoin join_;
  void* user_state_;

  std::vector<qbElement> elements_data_;
  std::vector<qbElement_> elements_;
  void* element_values_;

  qbTransform transform_;
  qbCallback callback_;
};


#endif  // SYSTEM_IMPL__H

