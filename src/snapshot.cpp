#include "snapshot.h"

Snapshot::Snapshot(int64_t timestamp_us, EntityRegistry* entities,
                   ComponentRegistry* components)
    : timestamp_us(timestamp_us) {
  this->entities.reset(entities->Clone());
  this->components.reset(components->Clone());
}