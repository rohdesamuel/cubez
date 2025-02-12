#include "entity_table.h"

#include <cubez/cubez.h>
#include <stdarg.h>

#include "instance_registry.h"
#include "game_state.h"
#include "component.h"

static qbComponent component_;

struct EntityTableComponent {
  qbEntityTable table;
};

void EntityTable::Initialize() {
  qbComponentAttr attr;
  qb_componentattr_create(&attr);
  qb_componentattr_setdatatype(attr, EntityTableComponent);
  qb_component_create(&component_, "EntityTableComponent", attr);
  qb_componentattr_destroy(&attr);

  qb_instance_ondestroy(component_, [](qbInstance instance, qbVar v) {
    EntityTableComponent* table_holder;
    qb_instance_get(instance, &table_holder);
    qb_entitytable_destroy(&table_holder->table);
  }, qbNil);
}

EntityTable::EntityTable(qbId id, qbEntityTableAttr attr, GameState* game_state, ComponentRegistry* component_registry):
  id_(id),
  game_state_(game_state),
  component_registry_(component_registry) {
  for (qbComponent component : attr->components) {
    ::Component* c = game_state->ComponentGet(component)->CloneEmpty();
    components_.insert(c->Id(), c);
  }
  main_component_ = components_[attr->components.front()];
}

qbComponent EntityTable::Component() {
  return component_;
}

EntityTable* EntityTable::FromRaw(qbEntityTable table) {
  return (EntityTable*)table->impl;
}

qbEntity EntityTable::insert(qbComponent component, void* data) {
  qbEntity entity;
  game_state_->Entities().CreateEntity(&entity);
  entity = SET_ENTITY_TABLE_ID(id_, entity);

  for (auto [id, c] : components_) {
    if (id == component) {
      c->Create(entity, data);
    } else {
      c->Create(entity, nullptr);
    }
  }

  return entity;
}

qbEntity EntityTable::insert(size_t count, void* pbufs[]) {
  qbEntity entity;
  game_state_->Entities().CreateEntity(&entity);
  entity = SET_ENTITY_TABLE_ID(id_, entity);

  size_t i = 0;
  for (auto [_, c] : components_) {
    if (i >= count) break;
    c->Create(entity, pbufs[i++]);
  }

  return entity;
}

qbEntity EntityTable::insert(va_list args) {
  qbEntity entity;
  game_state_->Entities().CreateEntity(&entity);
  entity = SET_ENTITY_TABLE_ID(id_, entity);

  uintptr_t p = va_arg(args, uintptr_t);
  for (auto [_, c] : components_) {
    if (p == 0xCD) break;
    c->Create(entity, (void*)p);
    p = va_arg(args, uintptr_t);
  }

  return entity;
}

void EntityTable::erase(qbEntity entity) {
  game_state_->EntityDestroy(entity);
}

void EntityTable::erase_all() {
  ::Component* c = (*components_.begin()).second;
  for (auto [entity, _] : *c) {
    component_registry_->SendInstanceDestroyNotification(entity, c, game_state_);
  }
}

void EntityTable::destroy_entity(qbEntity entity) {
  for (auto [_, c] : components_) {
    component_registry_->SendInstanceDestroyNotification(entity, c, game_state_);
  }
}

bool EntityTable::has_entity(qbEntity entity) {
  return main_component_->Has(entity);
}

bool EntityTable::has_component(qbComponent component) {
  return components_.has(component);
}

void* EntityTable::at(qbEntity entity, qbComponent component) {
  if (components_.has(component)) {
    return (*components_[component])[entity];
  }
  return NULL;
}

void EntityTable::reserve(size_t count) {
  for (auto [_, c] : components_) {
    c->Reserve(count);
  }
}

size_t EntityTable::count() const {
  return (*components_.begin()).second->Size();
}

Component* EntityTable::component(qbComponent c) {
  return components_[c];
}