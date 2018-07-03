#ifndef SNAPSHOT__H
#define SNAPSHOT__H

#include "component_registry.h"
#include "entity_registry.h"
#include <memory>

class Snapshot {
public:
  Snapshot(int64_t timestamp_us, EntityRegistry* entities, ComponentRegistry* components);

  std::unique_ptr<EntityRegistry> entities;
  std::unique_ptr<ComponentRegistry> components;
  const int64_t timestamp_us;
};

#endif  // SNAPSHOT__H