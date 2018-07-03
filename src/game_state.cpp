#include "game_state.h"
#include "private_universe.h"

GameState::GameState(std::unique_ptr<EntityRegistry> entities,
                     std::unique_ptr<ComponentRegistry> components)
  : entities_(std::move(entities)), components_(std::move(components)) {
  destroyed_entities_.resize(10);
  removed_components_.resize(10);
}


void GameState::Flush() {
  for (auto& removed_components : removed_components_) {
    for (auto& removed : removed_components) {
      EntityRemoveComponentInternal(removed.first, removed.second);
    }
    removed_components.clear();
  }

  for (auto& destroyed_entities : destroyed_entities_) {
    for (auto& destroyed : destroyed_entities) {
      EntityDestroyInternal(destroyed);
    }
    destroyed_entities.clear();
  }
}

qbResult GameState::EntityCreate(qbEntity* entity, const qbEntityAttr_& attr) {
  qbResult result = entities_->CreateEntity(entity, attr);
  components_->CreateInstancesFor(*entity, attr.component_list, this);
  return result;
}

qbResult GameState::EntityDestroy(qbEntity entity) {
  destroyed_entities_.resize(std::max(destroyed_entities_.size(), (size_t)PrivateUniverse::program_id + 1));
  destroyed_entities_[PrivateUniverse::program_id].push_back(entity);
  return QB_OK;
}

qbResult GameState::EntityDestroyInternal(qbEntity entity) {
  components_->DestroyInstancesFor(entity, this);
  qbResult result = entities_->DestroyEntity(entity);
  return result;
}

qbResult GameState::EntityFind(qbEntity* entity, qbId entity_id) {
  return entities_->Find(entity, entity_id);
}

bool GameState::EntityHasComponent(qbEntity entity, qbComponent component) {
  return (*components_)[component].Has(entity);
}

qbResult GameState::EntityAddComponent(qbEntity entity, qbComponent component,
                                       void* instance_data) {
  return components_->CreateInstanceFor(entity, component, instance_data, this);
}

qbResult GameState::EntityRemoveComponent(qbEntity entity, qbComponent component) {
  removed_components_.resize(std::max(removed_components_.size(), (size_t)PrivateUniverse::program_id + 1));
  removed_components_[PrivateUniverse::program_id].emplace_back(entity, component);
  return QB_OK;
}

qbResult GameState::EntityRemoveComponentInternal(qbEntity entity, qbComponent component) {
  return components_->DestroyInstanceFor(entity, component, this) == 1 ? QB_OK : QB_UNKNOWN;
}

qbResult GameState::ComponentCreate(qbComponent* component, qbComponentAttr attr) {
  return components_->Create(component, std::move(attr));
}

qbResult GameState::ComponentSubscribeToOnCreate(qbSystem system,
                                                 qbComponent component) {
  return components_->SubcsribeToOnCreate(system, component);
}

qbResult GameState::ComponentSubscribeToOnDestroy(qbSystem system,
                                                  qbComponent component) {
  return components_->SubcsribeToOnDestroy(system, component);
}

Component* GameState::ComponentGet(qbComponent component) {
  return &(*components_)[component];
}

void* GameState::ComponentGetEntityData(qbComponent component, qbEntity entity) {
  return (*components_)[component][entity];
}

size_t GameState::ComponentGetCount(qbComponent component) {
  return (*components_)[component].Size();
}

void GameState::AddMutableComponent(qbComponent component) {
  mutable_components_.insert(component);
}