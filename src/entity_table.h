#ifndef ENTITY_TABLE__H
#define ENTITY_TABLE__H

#include <cubez/cubez.h>
#include <stdarg.h>
#include <vector>

#include "component.h"
#include "component_registry.h"
#include "game_state.h"
#include "sparse_map.h"

class EntityTable {
public:
  EntityTable(qbId id, qbEntityTableAttr attr, GameState* game_state, ComponentRegistry* component_registry);

  inline qbId Id() const { return id_; }

  static void Initialize();
  static qbComponent Component();
  static EntityTable* FromRaw(qbEntityTable table);

  qbEntity insert(qbComponent component, void* data);
  qbEntity insert(size_t count, void* pbufs[]);
  qbEntity insert(va_list args);

  // Destroys the given entity and its associated components.
  void erase(qbEntity entity);

  // Destroys the given entity and its associated components.
  // This is called at the end of the frame when the entity is ready to be
  // destroyed and the associated OnDestroy event should be called.
  void destroy_entity(qbEntity entity);

  // Erases all entities in the table and sends OnDestroy events.
  void erase_all();

  // Returns true if the entity is in the table.
  bool has_entity(qbEntity entity);

  // Returns true if the component is in the table.
  bool has_component(qbComponent component);

  // Returns the entity from the given component.
  void* at(qbEntity entity, qbComponent component);

  void reserve(size_t count);

  // Returns the number of entities (rows) in the table.
  size_t count() const;

  qbResult find(qbEntity entity, qbComponent component, void* pbuf);

  ::Component* component(qbComponent c);

private:
  qbId id_;

  GameState* game_state_;
  ComponentRegistry* component_registry_;
  ::Component* main_component_;
  SparseMap<::Component*, std::vector<::Component*>> components_;
};

#endif  // ENTITY_TABLE__H