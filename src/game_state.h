#ifndef GAME_STATE__H
#define GAME_STATE__H

#include "component_registry.h"
#include "entity_registry.h"
#include <memory>
#include "sparse_map.h"

// Not thread-safe. Assumed to run in a single program.
class GameState {
public:
  GameState(std::unique_ptr<EntityRegistry> entities,
            std::unique_ptr<ComponentRegistry> components);

  void Flush();

  // Entity manipulation.
  qbResult EntityCreate(qbEntity* entity, const qbEntityAttr_& attr);
  qbResult EntityDestroy(qbEntity entity);
  qbResult EntityFind(qbEntity* entity, qbId entity_id);
  bool EntityHasComponent(qbEntity entity, qbComponent component);
  qbResult EntityAddComponent(qbEntity entity, qbComponent component,
                               void* instance_data);
  qbResult EntityRemoveComponent(qbEntity entity, qbComponent component);

  // Component manipulation.
  qbResult ComponentCreate(qbComponent* component, qbComponentAttr attr);
  qbResult ComponentSubscribeToOnCreate(qbSystem system, qbComponent component);
  qbResult ComponentSubscribeToOnDestroy(qbSystem system, qbComponent component);
  Component* ComponentGet(qbComponent component);
  void* ComponentGetEntityData(qbComponent component, qbEntity entity);
  size_t ComponentGetCount(qbComponent component);
  void AddMutableComponent(qbComponent component);

private:
  qbResult EntityRemoveComponentInternal(qbEntity entity, qbComponent component);
  qbResult EntityDestroyInternal(qbEntity entity);

  std::unique_ptr<EntityRegistry> entities_;
  std::unique_ptr<ComponentRegistry> components_;
  SparseSet mutable_components_;

  TypedBlockVector<std::vector<qbEntity>> destroyed_entities_;
  TypedBlockVector<std::vector<std::pair<qbEntity, qbComponent>>> removed_components_;

  friend class StateDelta;
};

#endif  // GAME_STATE__H