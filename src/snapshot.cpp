#include "snapshot.h"

Snapshot::Snapshot(int64_t timestamp_us, EntityRegistry* entities,
                   ComponentRegistry* components)
    : timestamp_us(timestamp_us) {
  this->entities = entities->Clone();
  this->components = components->Clone();
}

Snapshot::~Snapshot() {
  delete entities;
  delete components;
}