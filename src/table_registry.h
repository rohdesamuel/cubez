#ifndef TABLE_REGISTRY__H
#define TABLE_REGISTRY__H

#include <vector>
#include "component_registry.h"

class TableRegistry {
public:
  TableRegistry(ComponentRegistry* component_registry);

  void Init();
  qbResult Create(qbEntityTable* table, qbEntityTableAttr attr, class GameState* game_state);
  qbResult Destroy(qbEntityTable* table);
  class EntityTable* Find(qbId table_id);

  // Delay destroying the actual EntityTable b/c the entities in it need one
  // frame to be deleted.
  static void OnTableDestroy(void* event, qbVar v);

private:
  class ComponentRegistry* component_registry_;

  qbEvent destroy_event_;
  std::vector<EntityTable*> tables_;
};

#endif  // TABLE_REGISTRY__H