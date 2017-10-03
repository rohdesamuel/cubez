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
  *attr = (qbComponentAttr_*)calloc(1, sizeof(qbComponentAttr_));
	return qbResult::QB_OK;
}

qbResult qb_componentattr_destroy(qbComponentAttr* attr) {
  free(*attr);
  *attr = nullptr;
	return qbResult::QB_OK;
}

qbResult qb_componentattr_setprogram(qbComponentAttr* attr, const char* program) {
  (*attr)->program = program;
	return qbResult::QB_OK;
}

qbResult qb_componentattr_setdatasize(qbComponentAttr* attr, size_t size) {
  (*attr)->data_size = size;
	return qbResult::QB_OK;
}

qbResult qb_component_create(qbComponent* component, const char* name, qbComponentAttr attr) {
  attr->name = name;
  return AS_PRIVATE(component_create(component, attr));
}


qbResult qb_component_destroy(qbComponent* component) {
  free(*component);
  *component = nullptr;
	return qbResult::QB_OK;
}


qbResult qb_entityattr_create(qbEntityAttr* attr) {
  *attr = (qbEntityAttr)calloc(1, sizeof(qbEntityAttr_));
	return qbResult::QB_OK;
}

qbResult qb_entityattr_destroy(qbEntityAttr* attr) {
  free(*attr);
  *attr = nullptr;
	return qbResult::QB_OK;
}

qbResult qb_entityattr_addcomponent(qbEntityAttr* attr, qbComponent component,
                                    void* instance_data) {
  (*attr)->component_list.push_back({component, instance_data});
	return qbResult::QB_OK;
}

qbResult qb_entity_create(qbEntity* entity, qbEntityAttr attr) {
  return AS_PRIVATE(entity_create(entity, *attr));
}

qbResult qb_entity_destroy(qbEntity* entity) {
  free(*entity);
  *entity = nullptr;
	return qbResult::QB_OK;
}

qbResult qb_systemattr_create(qbSystemAttr* attr) {
  *attr = (qbSystemAttr_*)calloc(1, sizeof(qbSystemAttr_));
	return qbResult::QB_OK;
}

qbResult qb_systemattr_destroy(qbSystemAttr* attr) {
  free(*attr);
  *attr = nullptr;
	return qbResult::QB_OK;
}

qbResult qb_systemattr_setprogram(qbSystemAttr* attr, const char* program) {
  (*attr)->program = program;
	return qbResult::QB_OK;
}

qbResult qb_systemattr_addsource(qbSystemAttr* attr, qbComponent component) {
  (*attr)->sources.push_back(component);
	return qbResult::QB_OK;
}

qbResult qb_systemattr_addsink(qbSystemAttr* attr, qbComponent component) {
  (*attr)->sinks.push_back(component);
	return qbResult::QB_OK;
}

qbResult qb_systemattr_setfunction(qbSystemAttr* attr, qbTransform transform) {
  (*attr)->transform = transform;
	return qbResult::QB_OK;
}

qbResult qb_systemattr_setcallback(qbSystemAttr* attr, qbCallback callback) {
  (*attr)->callback = callback;
	return qbResult::QB_OK;
}

qbResult qb_systemattr_settrigger(qbSystemAttr* attr, qbTrigger trigger) {
  (*attr)->trigger = trigger;
	return qbResult::QB_OK;
}


qbResult qb_systemattr_setpriority(qbSystemAttr* attr, int16_t priority) {
  (*attr)->priority = priority;
	return qbResult::QB_OK;
}

qbResult qb_systemattr_setjoin(qbSystemAttr* attr, qbComponentJoin join) {
  (*attr)->join = join;
	return qbResult::QB_OK;
}

qbResult qb_systemattr_setuserstate(qbSystemAttr* attr, void* state) {
  (*attr)->state = state;
	return qbResult::QB_OK;
}


qbResult qb_system_create(qbSystem* system, qbSystemAttr attr) {
  AS_PRIVATE(system_create(system, *attr));
	return qbResult::QB_OK;
}

qbResult qb_system_destroy(qbSystem*) {
	return qbResult::QB_OK;
}


qbResult qb_collectionattr_create(qbCollectionAttr* attr) {
  (*attr) = (qbCollectionAttr)calloc(1, sizeof(qbCollectionAttr_));
	return qbResult::QB_OK;
}

qbResult qb_collectionattr_destroy(qbCollectionAttr* attr) {
  free(*attr);
  *attr = nullptr;
	return qbResult::QB_OK;
}

qbResult qb_collectionattr_setaccessors(qbCollectionAttr* attr, const char* program) {
  (*attr)->program = program;
	return qbResult::QB_OK;
}

qbResult qb_collectionattr_setaccessors(qbCollectionAttr* attr, qbValueByOffset by_offset,
                                        qbValueByKey by_key, qbValueByHandle by_handle) {
  (*attr)->accessor.offset = by_offset;
  (*attr)->accessor.key = by_key;
  (*attr)->accessor.handle = by_handle;
	return qbResult::QB_OK;
}

qbResult qb_collectionattr_setkeyiterator(qbCollectionAttr* attr, qbData data,
                                          size_t stride, uint32_t offset) {
  (*attr)->keys.data = data;
  (*attr)->keys.stride = stride;
  (*attr)->keys.offset = offset;
	return qbResult::QB_OK;
}

qbResult qb_collectionattr_setvalueiterator(qbCollectionAttr* attr, qbData data,
                                          size_t stride, uint32_t offset) {
  (*attr)->values.data = data;
  (*attr)->values.stride = stride;
  (*attr)->values.offset = offset;
	return qbResult::QB_OK;
}

qbResult qb_collectionattr_setupdate(qbCollectionAttr* attr, qbUpdate update) {
  (*attr)->update = update;
	return qbResult::QB_OK;
}

qbResult qb_collectionattr_setinsert(qbCollectionAttr* attr, qbInsert insert) {
  (*attr)->insert = insert;
	return qbResult::QB_OK;
}

qbResult qb_collectionattr_setcount(qbCollectionAttr* attr, qbCount count) {
  (*attr)->count = count;
	return qbResult::QB_OK;
}


qbResult qb_collection_create(qbCollection* collection, const char* name, qbCollectionAttr attr) {
  return AS_PRIVATE(collection_create(collection, name, attr));
}

qbResult qb_collection_share(qbCollection collection, qbProgram destination) {
	return AS_PRIVATE(collection_share(collection, destination));
}

qbResult qb_collection_copy(qbCollection collection, qbProgram destination) {
	return AS_PRIVATE(collection_copy(collection, destination));
}

qbResult qb_collection_destroy(qbCollection* collection) {
	return AS_PRIVATE(collection_destroy(collection));
}

qbResult qb_eventattr_create(qbEventAttr* attr) {
  *attr = (qbEventAttr)calloc(1, sizeof(qbEventAttr_));
	return qbResult::QB_OK;
}

qbResult qb_eventattr_destroy(qbEventAttr* attr) {
  free(*attr);
  *attr = nullptr;
	return qbResult::QB_OK;
}

qbResult qb_eventattr_setprogram(qbEventAttr* attr, const char* program) {
  (*attr)->program = program;
	return qbResult::QB_OK;
}

qbResult qb_eventattr_setmessagesize(qbEventAttr* attr, size_t size) {
  (*attr)->message_size = size;
	return qbResult::QB_OK;
}

qbResult qb_event_create(qbEvent* event, qbEventAttr attr) {
	return AS_PRIVATE(event_create(event, attr));
}

qbResult qb_event_destroy(qbEvent* event) {
	return AS_PRIVATE(event_destroy(event));
}

qbResult qb_event_flush(qbEvent event) {
	return AS_PRIVATE(event_flush(event));
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

