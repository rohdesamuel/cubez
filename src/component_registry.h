#ifndef COMPONENT_REGISTRY__H
#define COMPONENT_REGISTRY__H

#include "defs.h"

#include <atomic>
#include <unordered_map>

class ComponentRegistry {
 public:
  ComponentRegistry();

  qbResult CreateComponent(qbComponent* component, qbComponentAttr attr);

  static qbHandle CreateComponentInstance(
      qbComponent component, qbId entity_id, const void* value);

  static void DestroyComponentInstance(qbComponent component, qbHandle handle);

  static qbResult SendComponentCreateEvent(qbComponent component,
                                           qbEntity entity,
                                           void* instance_data);

  static qbResult SendComponentDestroyEvent(qbComponent component,
                                            qbEntity entity,
                                            void* instance_data);

 private:
  qbResult AllocComponent(qbComponent* component, qbComponentAttr attr);

  std::unordered_map<qbId, qbComponent> components_;
  std::atomic_long id_;
};

#endif  // COMPONENT_REGISTRY__H
