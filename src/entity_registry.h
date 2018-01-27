#ifndef ENTITY_REGISTRY__H
#define ENTITY_REGISTRY__H

#include "defs.h"
#include "memory_pool.h"
#include "component_registry.h"
#include "sparse_set.h"
#include "frame_buffer.h"

#include <algorithm>
#include <atomic>
#include <unordered_map>
#include <unordered_set>

class EntityRegistry {
 public:
  EntityRegistry(ComponentRegistry* component_registry, FrameBuffer* frame_buffer);

  void Init();
  EntityRegistry* Clone();

  // Creates an entity. Entity will be available for use next frame. Sends a
  // ComponentCreateEvent after all components have been created.
  qbResult CreateEntity(qbEntity* entity, const qbEntityAttr_& attr);

  // Destroys an entity and frees all components. Entity and components will be
  // destroyed next frame. Sends a ComponentDestroyEvent before components are
  // removed. Frees entity memory after all components have been destroyed.
  qbResult DestroyEntity(qbEntity entity);

  qbResult AddComponent(qbId entity, qbComponent component,
                        void* instance_data);

  qbResult RemoveComponent(qbId entity, qbComponent component);

  qbResult Find(qbEntity* entity, qbId entity_id);

  void Resolve(const std::vector<qbEntity>& created,
               const std::vector<qbEntity>& destroyed);

 private:
  qbId AllocEntity();

  std::atomic_long id_;
  
  FrameBuffer* frame_buffer_;
  ComponentRegistry* component_registry_;

  SparseSet entities_;
  std::vector<size_t> free_entity_ids_;

};

#endif  // ENTITY_REGISTRY__H

