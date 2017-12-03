#ifndef ENTITY_REGISTRY__H
#define ENTITY_REGISTRY__H

#include "defs.h"

#include <algorithm>
#include <atomic>
#include <unordered_map>

class EntityRegistry {
 public:
  struct DestroyEntityEvent {
    EntityRegistry* self;
    qbId entity_id;
  };

  struct AddComponentEvent {
    qbEntity entity;
    qbComponent component;
    void* instance_data;
  };

  struct RemoveComponentEvent {
    qbEntity entity;
    qbComponent component;
  };

  EntityRegistry();

  void Init();

  // Creates an entity. Entity will be available for use next frame. Sends a
  // ComponentCreateEvent after all components have been created.
  qbResult CreateEntity(qbEntity* entity, const qbEntityAttr_& attr);

  // Destroys an entity and frees all components. Entity and components will be
  // destroyed next frame. Sends a ComponentDestroyEvent before components are
  // removed. Frees entity memory after all components have been destroyed.
  qbResult DestroyEntity(qbEntity* entity);

  void SetComponents(qbEntity entity,
                      const std::vector<qbComponentInstance_>& components);

  qbResult AddComponent(qbEntity entity, qbComponent component,
                        void* instance_data);

  qbResult RemoveComponent(qbEntity entity, qbComponent component);

  qbResult Find(qbEntity* entity, qbId entity_id);

 private:
  qbResult AllocEntity(qbEntity* entity);

  qbResult SendDestroyEntityEvent(qbEntity entity);
  
  qbResult SendRemoveComponentEvent(qbEntity entity, qbComponent component);

  static void AddComponentHandler(qbCollectionInterface*, qbFrame* frame);

  static void RemoveComponentHandler(qbCollectionInterface*,
                                       qbFrame* frame);

  static void DestroyEntityHandler(qbCollectionInterface*, qbFrame* frame);

  std::unordered_map<qbId, qbEntity> entities_;
  std::atomic_long id_;

  qbEvent add_component_event_;
  qbEvent remove_component_event_;
  qbEvent destroy_entity_event_;

  qbSystem add_component_system_;
  qbSystem remove_component_system_;
  qbSystem destroy_entity_system_;
};

#endif  // ENTITY_REGISTRY__H

