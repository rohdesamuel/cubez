#include "frame_buffer.h"
#include "entity_registry.h"
#include "private_universe.h"
#include "component.h"

void FrameBuffer::Init(EntityRegistry* entity_registry) {
  entity_registry_ = entity_registry; 
}

void FrameBuffer::RegisterProgram(qbId program) {
  state_[program] = FrameState{};
}

void FrameBuffer::RegisterComponent(qbId program, class Component* component) {
  state_[program].components_[component->Id()] = component->MakeBuffer();
}

const SparseMap<ComponentBuffer*>& FrameBuffer::ComponentBuffers() const {
  return state_.at(PrivateUniverse::program_id).components_;
}

ComponentBuffer* FrameBuffer::Component(qbId component_id) {
  return state_.at(PrivateUniverse::program_id).components_[component_id];
}

void FrameBuffer::ResolveDeltas() {
  for (auto& pair : state_) {
    entity_registry_->Resolve(pair.second.created_entities_,
                              pair.second.destroyed_entities_);
    pair.second.created_entities_.resize(0);
    pair.second.destroyed_entities_.resize(0);
  }

  for (auto& pair : state_) {
    for (auto& buffer_pair : pair.second.components_) {
      buffer_pair.second->Resolve();
    }
  }
}

void FrameBuffer::CreateEntity(qbEntity entity) {
  state_.at(PrivateUniverse::program_id).created_entities_.push_back(entity);
}

void FrameBuffer::DestroyEntity(qbEntity entity) {
  state_.at(PrivateUniverse::program_id).destroyed_entities_.push_back(entity);
}