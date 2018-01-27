#ifndef FRAME_BUFFER__H
#define FRAME_BUFFER__H

#include "defs.h"

#include <unordered_map>

class ComponentBuffer;
class EntityRegistry;
class FrameBuffer {
public:
  void Init(EntityRegistry* entity_registry);

  void RegisterProgram(qbId program);

  void RegisterComponent(qbId program, class Component* component);

  const SparseMap<ComponentBuffer*, std::vector<ComponentBuffer*>>& ComponentBuffers() const;

  ComponentBuffer* Component(qbId component_id);

  void CreateEntity(qbEntity entity);

  void DestroyEntity(qbEntity entity);

  void ResolveDeltas();

private:
  struct FrameState {
    SparseMap<ComponentBuffer*, std::vector<ComponentBuffer*>> components_;
    std::vector<qbEntity> created_entities_;
    std::vector<qbEntity> destroyed_entities_;
  };

  EntityRegistry* entity_registry_;

  SparseMap<FrameState, std::vector<FrameState>> state_;
};

#endif  // FRAME_BUFFER__H
