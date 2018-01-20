#include "cubez.h"
#include "defs.h"
#include "private_universe.h"
#include "byte_vector.h"
#include "component.h"
#include "system_impl.h"

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
  return AS_PRIVATE(entity_create(entity, *attr));
}

qbResult qb_entity_destroy(qbEntity entity) {
  return AS_PRIVATE(entity_destroy(entity));
}

bool qb_entity_hascomponent(qbEntity entity, qbComponent component) {
  return component->instances.Has(entity);
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

qbResult qb_systemattr_addconst(qbSystemAttr attr, qbComponent component) {
  attr->constants.push_back(component);
  attr->components.push_back(component);
	return qbResult::QB_OK;
}

qbResult qb_systemattr_addmutable(qbSystemAttr attr, qbComponent component) {
  attr->mutables.push_back(component);
  attr->components.push_back(component);
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
    qbInstance_ instance = SystemImpl::FromRaw(frame->system)->FindInstance(event->entity, event->component);
    on_create(&instance);
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
    qbInstance_ instance = SystemImpl::FromRaw(frame->system)->FindInstance(event->entity, event->component);
    on_destroy(&instance);
  });
  qbSystem system;
  qb_system_create(&system, attr);

  component->instances.SubcsribeToOnDestroy(system);

  qb_systemattr_destroy(&attr);
  return QB_OK;
}

qbEntity qb_instance_getentity(qbInstance instance) {
  return instance->entity;
}

qbResult qb_instance_getconst(qbInstance instance, void* pbuffer) {
#ifdef __ENGINE_DEBUG__
  if (!instance->is_mutable) {
    *(void**)pbuffer = instance->data;
  } else {
    *(void**)pbuffer = nullptr;
  }
#endif
  *(void**)pbuffer = instance->data;
  return QB_OK;
}

qbResult qb_instance_getmutable(qbInstance instance, void* pbuffer) {
#ifdef __ENGINE_DEBUG__
  if (instance->is_mutable) {
    *(void**)pbuffer = instance->data;
  } else {
    *(void**)pbuffer = nullptr;
  }
#endif
  *(void**)pbuffer = instance->data;
  return QB_OK;
}

qbResult qb_instance_getcomponent(qbInstance instance, qbComponent component, void* pbuffer) {
  *(void**)pbuffer = SystemImpl::FromRaw(instance->system)->
    FindInstanceData(instance, component);
  return QB_OK;
}

bool qb_instance_hascomponent(qbInstance instance, qbComponent component) {
  return component->instances.Has(instance->entity);
}