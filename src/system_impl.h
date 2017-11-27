#ifndef SYSTEM_IMPL__H
#define SYSTEM_IMPL__H

#include "defs.h"

#include <algorithm>
#include <cstring>

class SystemImpl {
 public:
  SystemImpl(const qbSystemAttr_& attr, qbSystem system);

  static SystemImpl* FromRaw(qbSystem system);
  
  void Run(void* event = nullptr);

 private:
  void CopyToElement(void* k, void* v, qbOffset offset, qbElement element);
  void CopyToElement(void* k, void* v, qbElement element);

  void Run_0_To_0(qbFrame* f);
  void Run_1_To_0(qbFrame* f);
  void Run_0_To_N(qbFrame* f);
  void Run_1_To_N(qbFrame* f);
  void Run_M_To_N(qbFrame* f);

  qbSystem system_;
  std::vector<qbCollection> sources_;
  std::vector<qbCollection> sinks_;
  std::vector<qbCollectionInterface> source_interfaces_;
  std::vector<qbCollectionInterface> sink_interfaces_;

  qbComponentJoin join_;
  void* user_state_;

  std::vector<qbElement> elements_data_;
  std::vector<qbElement_> elements_;
  void* element_values_;

  qbTransform transform_;
  qbCallback callback_;
};


#endif  // SYSTEM_IMPL__H

