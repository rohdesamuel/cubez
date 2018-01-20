#include "component_registry.h"

#include "component.h"

ComponentRegistry::ComponentRegistry() : id_(0) {}

void ComponentRegistry::Init() {}

qbResult ComponentRegistry::ComponentCreate(qbComponent* component, qbComponentAttr attr,
                                            FrameBuffer* frame_buffer) {
  qbId new_id = id_++;
  *component = (qbComponent)malloc(sizeof(qbComponent_));
  new (*component) qbComponent_{Component(frame_buffer, *component, new_id, attr->data_size)};

  components_[new_id] = *component;
  *component = components_[new_id];
  return qbResult::QB_OK;
}

qbResult ComponentRegistry::CreateInstancesFor(
    qbEntity entity, const std::vector<qbComponentInstance_>& instances) {
  for (auto& instance : instances) {
    instance.component->instances.Create(entity, instance.data);
  }

  for (auto& instance : instances) {
    instance.component->instances.SendInstanceCreateNotification(entity);
  }

  return QB_OK;
}

qbResult ComponentRegistry::CreateInstanceFor(qbEntity entity,
                                              qbComponent component,
                                              void* instance_data) {
  component->instances.Create(entity, instance_data);
  component->instances.SendInstanceCreateNotification(entity);
  return QB_OK;
}

int ComponentRegistry::DestroyInstancesFor(qbEntity entity) {
  int destroyed_instances = 0;
  for (auto it = components_.begin(); it != components_.end(); ++it) {
    qbComponent component = (*it).second;
    if (component->instances.Has(entity)) {
      component->instances.SendInstanceDestroyNotification(entity);
    }
  }

  for (auto it = components_.begin(); it != components_.end(); ++it) {
    qbComponent component = (*it).second;
    if (component->instances.Has(entity)) {
      component->instances.Destroy(entity);
      ++destroyed_instances;
    }
  }
  return destroyed_instances;
}

qbResult ComponentRegistry::DestroyInstanceFor(qbEntity entity,
                                               qbComponent component) {
  component->instances.SendInstanceDestroyNotification(entity);
  component->instances.Destroy(entity);
  return QB_OK;
}
