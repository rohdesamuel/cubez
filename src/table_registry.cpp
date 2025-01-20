#include "table_registry.h"

#include "defs.h"
#include "entity_table.h"
#include "instance_registry.h"
#include "game_state.h"

struct TableDestroyEvent {
  EntityTable* to_delete;
};

TableRegistry::TableRegistry(ComponentRegistry* component_registry): component_registry_(component_registry) {

}

void TableRegistry::Init() {
  qbEventAttr attr;
  qb_eventattr_create(&attr);
  qb_eventattr_setmessagetype(attr, TableDestroyEvent);
  qb_event_create(&destroy_event_, attr);

  qb_event_subscribe(destroy_event_, OnTableDestroy, qbPtr(this));
}

void TableRegistry::OnTableDestroy(void* event, qbVar v) {
  TableDestroyEvent* e = (TableDestroyEvent*)event;
  e->to_delete->erase_all();
  delete e->to_delete;
}

qbResult TableRegistry::Create(qbEntityTable* table, qbEntityTableAttr attr, GameState* game_state) {
  if (tables_.size() == QB_MAX_TABLES_COUNT) {
    *table = NULL;
    return QB_ERROR_MAX_TABLES_REACHED;
  }

  EntityTable* impl = new EntityTable(tables_.size(), attr, game_state, component_registry_);
  *table = new qbEntityTable_{};
  (*table)->impl = impl;
  tables_.push_back(impl);
  return QB_OK;
}

qbResult TableRegistry::Destroy(qbEntityTable* table) {
  EntityTable* impl = EntityTable::FromRaw(*table);

  // Entities are destroyed at the end of the frame regardless of when the
  // event was sent. So, send the event and destroy the entities in the
  // handler.
  TableDestroyEvent destroy_event{ .to_delete = impl };
  qb_event_send(destroy_event_, &destroy_event);

  delete *table;
  *table = nullptr;
  return QB_OK;
}

EntityTable* TableRegistry::Find(qbId table_id) {
  return tables_[table_id];
}
