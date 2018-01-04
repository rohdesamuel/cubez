#include "cubez.h"
#include "defs.h"
#include "private_universe.h"
#include "byte_vector.h"
#include "component.h"

#define AS_PRIVATE(expr) ((PrivateUniverse*)(universe_->self))->expr

static qbUniverse* universe_ = nullptr;

qbResult qb_init(qbUniverse* u) {
  universe_ = u;
  universe_->self = new PrivateUniverse();

  return AS_PRIVATE(init());
}

qbResult qb_start() {
  return AS_PRIVATE(start());
}

qbResult qb_stop() {
  qbResult ret = AS_PRIVATE(stop());
  universe_ = nullptr;
  return ret;
}

qbResult qb_loop() {
  return AS_PRIVATE(loop());
}

qbId qb_create_program(const char* name) {
  return AS_PRIVATE(create_program(name));
}

qbResult qb_run_program(qbId program) {
  return AS_PRIVATE(run_program(program));
}

qbResult qb_detach_program(qbId program) {
  return AS_PRIVATE(detach_program(program));
}

qbResult qb_join_program(qbId program) {
  return AS_PRIVATE(join_program(program));
}

qbResult qb_system_enable(qbSystem system) {
  return AS_PRIVATE(enable_system(system));
}

qbResult qb_system_disable(qbSystem system) {
  return AS_PRIVATE(disable_system(system));
}

qbResult qb_componentattr_create(qbComponentAttr* attr) {
  *attr = (qbComponentAttr)calloc(1, sizeof(qbComponentAttr_));
  new (*attr) qbComponentAttr_;
	return qbResult::QB_OK;
}

qbResult qb_componentattr_destroy(qbComponentAttr* attr) {
  delete *attr;
  *attr = nullptr;
	return qbResult::QB_OK;
}

qbResult qb_componentattr_setprogram(qbComponentAttr attr, const char* program) {
  attr->program = program;
	return qbResult::QB_OK;
}

qbResult qb_componentattr_setdatasize(qbComponentAttr attr, size_t size) {
  attr->data_size = size;
	return qbResult::QB_OK;
}

qbResult qb_component_create(
    qbComponent* component, qbComponentAttr attr) {
  if (!attr->program) {
    attr->program = "";
  }
  return AS_PRIVATE(component_create(component, attr));
}

qbResult qb_component_destroy(qbComponent*) {
	return qbResult::QB_OK;
}

size_t qb_component_getcount(qbComponent component) {
  return component->instances.Size();
}

qbResult qb_instance_oncreate(qbComponent component,
                               qbInstanceOnCreate on_create) {
  qbSystemAttr attr;
  qb_systemattr_create(&attr);
  qb_systemattr_settrigger(attr, qbTrigger::QB_TRIGGER_EVENT);
  qb_systemattr_setuserstate(attr, (void*)on_create);
  qb_systemattr_setcallback(attr, [](qbFrame* frame) {
        qbInstanceOnCreateEvent_* event =
            (qbInstanceOnCreateEvent_*)frame->event;
        qbInstanceOnCreate on_create = (qbInstanceOnCreate)frame->state;
        on_create(event->entity, event->component,
                  event->component->instances[event->entity]);
      });
  qbSystem system;
  qb_system_create(&system, attr);

  component->instances.SubcsribeToOnCreate(system);

  qb_systemattr_destroy(&attr);
  return QB_OK;
}

qbResult qb_instance_ondestroy(qbComponent component,
                                qbInstanceOnDestroy on_destroy) {
  qbSystemAttr attr;
  qb_systemattr_create(&attr);
  qb_systemattr_settrigger(attr, qbTrigger::QB_TRIGGER_EVENT);
  qb_systemattr_setuserstate(attr, (void*)on_destroy);
  qb_systemattr_setcallback(attr, [](qbFrame* frame) {
        qbInstanceOnDestroyEvent_* event =
            (qbInstanceOnDestroyEvent_*)frame->event;
        qbInstanceOnDestroy on_destroy = (qbInstanceOnDestroy)frame->state;
        on_destroy(event->entity, event->component,
                   event->component->instances[event->entity]);
      });
  qbSystem system;
  qb_system_create(&system, attr);

  component->instances.SubcsribeToOnDestroy(system);

  qb_systemattr_destroy(&attr);
  return QB_OK;
}

qbResult qb_entityattr_create(qbEntityAttr* attr) {
  *attr = (qbEntityAttr)calloc(1, sizeof(qbEntityAttr_));
  new (*attr) qbEntityAttr_;
	return qbResult::QB_OK;
}

qbResult qb_entityattr_destroy(qbEntityAttr* attr) {
  delete *attr;
  *attr = nullptr;
	return qbResult::QB_OK;
}

qbResult qb_entityattr_addcomponent(qbEntityAttr attr, qbComponent component,
                                    void* instance_data) {
  attr->component_list.push_back({component, instance_data});
	return qbResult::QB_OK;
}

qbResult qb_entity_create(qbEntity* entity, qbEntityAttr attr) {
#ifdef __ENGINE_DEBUG__
  DEBUG_ASSERT(!attr->component_list.empty(),
               QB_ERROR_ENTITYATTR_COMPONENTS_ARE_EMPTY);
#endif
  return AS_PRIVATE(entity_create(entity, *attr));
}

qbResult qb_entity_destroy(qbEntity* entity) {
  if (entity) {
    return AS_PRIVATE(entity_destroy(entity));
  }
  return QB_ERROR_NULL_POINTER;
}

qbResult qb_entity_find(qbEntity* entity, qbId entity_id) {
  return AS_PRIVATE(entity_find(entity, entity_id));
}

qbResult qb_entity_getcomponent(qbEntity entity, qbComponent component,
                                void* buffer) {
  if (component->instances.Has(entity)) {
    *(void**)buffer = component->instances[entity];
  } else {
    *(void**)buffer = nullptr;
  }
  return QB_OK;
}

qbResult qb_entity_hascomponent(qbEntity entity, qbComponent component) {
  return component->instances.Has(entity) ? QB_OK : QB_ERROR_NOT_FOUND;
}

qbResult qb_entity_addcomponent(qbEntity entity, qbComponent component,
                                void* instance_data) {
  return AS_PRIVATE(entity_addcomponent(entity, component, instance_data));
}

qbResult qb_entity_removecomponent(qbEntity entity, qbComponent component) {
  return AS_PRIVATE(entity_removecomponent(entity, component));
}

qbId qb_entity_getid(qbEntity entity) {
  return entity;
}

qbResult qb_systemattr_create(qbSystemAttr* attr) {
  *attr = (qbSystemAttr)calloc(1, sizeof(qbSystemAttr_));
  new (*attr) qbSystemAttr_;
	return qbResult::QB_OK;
}

qbResult qb_systemattr_destroy(qbSystemAttr* attr) {
  delete *attr;
  *attr = nullptr;
	return qbResult::QB_OK;
}

qbResult qb_systemattr_setprogram(qbSystemAttr attr, const char* program) {
  attr->program = program;
	return qbResult::QB_OK;
}

qbResult qb_systemattr_addsource(qbSystemAttr attr, qbComponent component) {
  attr->sources.push_back(component);
	return qbResult::QB_OK;
}

qbResult qb_systemattr_addsink(qbSystemAttr attr, qbComponent component) {
  attr->sinks.push_back(component);
	return qbResult::QB_OK;
}

qbResult qb_systemattr_setfunction(qbSystemAttr attr, qbTransform transform) {
  attr->transform = transform;
	return qbResult::QB_OK;
}

qbResult qb_systemattr_setcallback(qbSystemAttr attr, qbCallback callback) {
  attr->callback = callback;
	return qbResult::QB_OK;
}

qbResult qb_systemattr_settrigger(qbSystemAttr attr, qbTrigger trigger) {
  attr->trigger = trigger;
	return qbResult::QB_OK;
}


qbResult qb_systemattr_setpriority(qbSystemAttr attr, int16_t priority) {
  attr->priority = priority;
	return qbResult::QB_OK;
}

qbResult qb_systemattr_setjoin(qbSystemAttr attr, qbComponentJoin join) {
  attr->join = join;
	return qbResult::QB_OK;
}

qbResult qb_systemattr_setuserstate(qbSystemAttr attr, void* state) {
  attr->state = state;
	return qbResult::QB_OK;
}


qbResult qb_system_create(qbSystem* system, qbSystemAttr attr) {
  if (!attr->program) {
    attr->program = "";
  }
#ifdef __ENGINE_DEBUG__
  DEBUG_ASSERT(attr->transform || attr->callback,
               qbResult::QB_ERROR_SYSTEMATTR_HAS_FUNCTION_OR_CALLBACK);
#endif
  AS_PRIVATE(system_create(system, *attr));
	return qbResult::QB_OK;
}

qbResult qb_system_destroy(qbSystem*) {
	return qbResult::QB_OK;
}


qbResult qb_collectionattr_create(qbCollectionAttr* attr) {
  *attr = (qbCollectionAttr)calloc(1, sizeof(qbCollectionAttr_));
  new (*attr) qbCollectionAttr_;
	return qbResult::QB_OK;
}

qbResult qb_collectionattr_destroy(qbCollectionAttr* attr) {
  delete *attr;
  *attr = nullptr;
	return qbResult::QB_OK;
}

qbResult qb_collectionattr_setimplementation(qbCollectionAttr attr, void* impl) {
  attr->collection = impl;
	return qbResult::QB_OK;
}

qbResult qb_collectionattr_setprogram(qbCollectionAttr attr, const char* program) {
  attr->program = program;
	return qbResult::QB_OK;
}

qbResult qb_collectionattr_setaccessors(qbCollectionAttr attr, qbValueByOffset by_offset,
                                        qbValueById by_id, qbValueByHandle by_handle) {
  attr->accessor.offset = by_offset;
  attr->accessor.id = by_id;
  attr->accessor.handle = by_handle;
	return qbResult::QB_OK;
}

qbResult qb_collectionattr_setremovers(qbCollectionAttr attr, qbRemoveByOffset by_offset,
                                        qbRemoveById by_id, qbRemoveByHandle by_handle) {
  attr->remove_by_offset = by_offset;
  attr->remove_by_id = by_id;
  attr->remove_by_handle = by_handle;
	return qbResult::QB_OK;
}

qbResult qb_collectionattr_setkeyiterator(qbCollectionAttr attr, qbData data,
                                          size_t stride, uint32_t offset) {
  attr->keys.data = data;
  attr->keys.stride = stride;
  attr->keys.offset = offset;
  attr->keys.size = sizeof(qbId);
	return qbResult::QB_OK;
}

qbResult qb_collectionattr_setvalueiterator(qbCollectionAttr attr, qbData data,
                                            size_t size, size_t stride,
                                            uint32_t offset) {
  attr->values.data = data;
  attr->values.stride = stride;
  attr->values.offset = offset;
  attr->values.size = size;
	return qbResult::QB_OK;
}

qbResult qb_collectionattr_setinsert(qbCollectionAttr attr, qbInsert insert) {
  attr->insert = insert;
	return qbResult::QB_OK;
}

qbResult qb_collectionattr_setcount(qbCollectionAttr attr, qbCount count) {
  attr->count = count;
	return qbResult::QB_OK;
}


qbResult qb_collection_create(qbCollection* collection, qbCollectionAttr attr) {
  if (!attr->program) {
    attr->program = "";
  }
#ifdef __ENGINE_DEBUG__
  DEBUG_ASSERT(attr->remove_by_id,
               QB_ERROR_COLLECTIONATTR_REMOVE_BY_KEY_IS_NOT_SET);
  DEBUG_ASSERT(attr->remove_by_offset,
               QB_ERROR_COLLECTIONATTR_REMOVE_BY_OFFSET_IS_NOT_SET);
  DEBUG_ASSERT(attr->remove_by_handle,
               QB_ERROR_COLLECTIONATTR_REMOVE_BY_HANDLE_IS_NOT_SET);
  DEBUG_ASSERT(attr->accessor.id,
               QB_ERROR_COLLECTIONATTR_ACCESSOR_KEY_IS_NOT_SET);
  DEBUG_ASSERT(attr->accessor.offset,
               QB_ERROR_COLLECTIONATTR_ACCESSOR_OFFSET_IS_NOT_SET);
  DEBUG_ASSERT(attr->accessor.handle,
               QB_ERROR_COLLECTIONATTR_ACCESSOR_HANDLE_IS_NOT_SET);
  DEBUG_ASSERT(attr->keys.data,
               QB_ERROR_COLLECTIONATTR_KEYITERATOR_DATA_IS_NOT_SET);
  DEBUG_ASSERT(attr->keys.stride > 0,
               QB_ERROR_COLLECTIONATTR_KEYITERATOR_STRIDE_IS_NOT_SET);
  DEBUG_ASSERT(attr->values.data,
               QB_ERROR_COLLECTIONATTR_VALUEITERATOR_DATA_IS_NOT_SET);
  DEBUG_ASSERT(attr->values.stride > 0,
               QB_ERROR_COLLECTIONATTR_VALUEITERATOR_STRIDE_IS_NOT_SET);
  DEBUG_ASSERT(attr->insert, QB_ERROR_COLLECTIONATTR_INSERT_IS_NOT_SET);
  DEBUG_ASSERT(attr->count, QB_ERROR_COLLECTIONATTR_COUNT_IS_NOT_SET);
  DEBUG_ASSERT(attr->collection,
               QB_ERROR_COLLECTIONATTR_IMPLEMENTATION_IS_NOT_SET);
#endif
  return AS_PRIVATE(collection_create(collection, attr));
}

qbResult qb_collection_share(qbCollection collection, qbProgram destination) {
	return AS_PRIVATE(collection_share(collection, destination));
}

qbResult qb_collection_destroy(qbCollection* collection) {
	return AS_PRIVATE(collection_destroy(collection));
}

qbResult qb_eventattr_create(qbEventAttr* attr) {
  *attr = (qbEventAttr)calloc(1, sizeof(qbEventAttr_));
  new (*attr) qbEventAttr_;
	return qbResult::QB_OK;
}

qbResult qb_eventattr_destroy(qbEventAttr* attr) {
  delete *attr;
  *attr = nullptr;
	return qbResult::QB_OK;
}

qbResult qb_eventattr_setprogram(qbEventAttr attr, const char* program) {
  attr->program = program;
	return qbResult::QB_OK;
}

qbResult qb_eventattr_setmessagesize(qbEventAttr attr, size_t size) {
  attr->message_size = size;
	return qbResult::QB_OK;
}

qbResult qb_event_create(qbEvent* event, qbEventAttr attr) {
  if (!attr->program) {
    attr->program = "";
  }
#ifdef __ENGINE_DEBUG__
  DEBUG_ASSERT(attr->message_size > 0,
               qbResult::QB_ERROR_EVENTATTR_MESSAGE_SIZE_IS_ZERO);
#endif
	return AS_PRIVATE(event_create(event, attr));
}

qbResult qb_event_destroy(qbEvent* event) {
	return AS_PRIVATE(event_destroy(event));
}

qbResult qb_event_flushall(qbProgram program) {
	return AS_PRIVATE(event_flushall(program));
}

qbResult qb_event_subscribe(qbEvent event, qbSystem system) {
	return AS_PRIVATE(event_subscribe(event, system));
}

qbResult qb_event_unsubscribe(qbEvent event, qbSystem system) {
	return AS_PRIVATE(event_unsubscribe(event, system));
}

qbResult qb_event_send(qbEvent event, void* message) {
  return AS_PRIVATE(event_send(event, message));
}

qbResult qb_event_sendsync(qbEvent event, void* message) {
  return AS_PRIVATE(event_sendsync(event, message));
}

qbId qb_element_getid(qbElement element) {
  return element->id;
}

qbEntity qb_element_getentity(qbElement element) {
  qbEntity ret;
  qb_entity_find(&ret, element->id);
  return ret;
}

qbResult qb_element_read(qbElement element, void* buffer) {
  memmove(buffer,
          element->read_buffer,
          element->size);
  element->user_buffer = buffer;
  return QB_OK;
}

qbResult qb_element_write(qbElement element) {
  memmove(element->component->instances[element->id],
          element->user_buffer, element->size);
  return QB_OK;
}
