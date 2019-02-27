#include <cubez/cubez.h>
#include <cubez/utils.h>
#include "defs.h"
#include "private_universe.h"
#include "byte_vector.h"
#include "component.h"
#include "system_impl.h"
#include "utils_internal.h"
#include "coro_scheduler.h"

#define AS_PRIVATE(expr) ((PrivateUniverse*)(universe_->self))->expr

const qbVar qbNone = { QB_TAG_VOID, 0 };
const qbVar qbFuture = { QB_TAG_UNSET, 0 };

static qbUniverse* universe_ = nullptr;
CoroScheduler* coro_scheduler;
Coro coro_main;

qbResult qb_init(qbUniverse* u) {
  universe_ = u;

  utils_initialize();
  coro_main = coro_initialize(u);
  
  universe_->self = new PrivateUniverse();
  coro_scheduler = new CoroScheduler(4);

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
  qbResult result = AS_PRIVATE(loop());
  coro_scheduler->run_sync();

  return result;
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
  (*attr)->is_shared = false;
  (*attr)->type = qbComponentType::QB_COMPONENT_TYPE_RAW;
	return qbResult::QB_OK;
}

qbResult qb_componentattr_destroy(qbComponentAttr* attr) {
  delete *attr;
  *attr = nullptr;
	return qbResult::QB_OK;
}

qbResult qb_componentattr_setdatasize(qbComponentAttr attr, size_t size) {
  attr->data_size = size;
  return qbResult::QB_OK;
}

qbResult qb_componentattr_settype(qbComponentAttr attr, qbComponentType type) {
  attr->type = type;
	return qbResult::QB_OK;
}

qbResult qb_componentattr_setshared(qbComponentAttr attr) {
  attr->is_shared = true;
  return qbResult::QB_OK;
}

qbResult qb_component_create(
    qbComponent* component, qbComponentAttr attr) {
  return AS_PRIVATE(component_create(component, attr));
}

qbResult qb_component_destroy(qbComponent*) {
	return qbResult::QB_OK;
}

size_t qb_component_getcount(qbComponent component) {
  return AS_PRIVATE(component_getcount(component));
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
  return AS_PRIVATE(entity_hascomponent(entity, component));
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

qbResult qb_barrier_create(qbBarrier* barrier) {
  *barrier = AS_PRIVATE(barrier_create());
  return QB_OK;
}

qbResult qb_barrier_destroy(qbBarrier* barrier) {
  AS_PRIVATE(barrier_destroy(*barrier));
  return QB_OK;
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

qbResult qb_systemattr_setprogram(qbSystemAttr attr, qbId program) {
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

qbResult qb_systemattr_setcondition(qbSystemAttr attr, qbCondition condition) {
  attr->condition = condition;
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

qbResult qb_systemattr_addbarrier(qbSystemAttr attr,
                                  qbBarrier barrier) {
  qbTicket_* t = new qbTicket_;
  t->impl = ((Barrier*)barrier->impl)->MakeTicket().release();

  t->lock = [ticket{ t->impl }]() { ((Barrier::Ticket*)ticket)->lock(); };
  t->unlock = [ticket{ t->impl }]() { ((Barrier::Ticket*)ticket)->unlock(); };
  attr->tickets.push_back(t);
  return QB_OK;
}

qbResult qb_system_create(qbSystem* system, qbSystemAttr attr) {
  if (!attr->program) {
    attr->program = 0;
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

qbResult qb_eventattr_setprogram(qbEventAttr attr, qbId program) {
  attr->program = program;
	return qbResult::QB_OK;
}

qbResult qb_eventattr_setmessagesize(qbEventAttr attr, size_t size) {
  attr->message_size = size;
	return qbResult::QB_OK;
}

qbResult qb_event_create(qbEvent* event, qbEventAttr attr) {
  if (!attr->program) {
    attr->program = 0;
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
  return AS_PRIVATE(instance_oncreate(component, on_create));
}

qbResult qb_instance_ondestroy(qbComponent component,
                               qbInstanceOnDestroy on_destroy) {
  return AS_PRIVATE(instance_ondestroy(component, on_destroy));
}

qbEntity qb_instance_getentity(qbInstance instance) {
  return instance->entity;
}

qbResult qb_instance_getconst(qbInstance instance, void* pbuffer) {
  return AS_PRIVATE(instance_getconst(instance, pbuffer));
}

qbResult qb_instance_getmutable(qbInstance instance, void* pbuffer) {
  return AS_PRIVATE(instance_getmutable(instance, pbuffer));
}

qbResult qb_instance_getcomponent(qbInstance instance, qbComponent component, void* pbuffer) {
  return AS_PRIVATE(instance_getcomponent(instance, component, pbuffer));
}

bool qb_instance_hascomponent(qbInstance instance, qbComponent component) {
  return AS_PRIVATE(instance_hascomponent(instance, component));
}

qbResult qb_instance_find(qbComponent component, qbEntity entity, void* pbuffer) {
  return AS_PRIVATE(instance_find(component, entity, pbuffer));
}

qbCoro qb_coro_create(qbVar(*entry)(qbVar var)) {
  qbCoro ret = new qbCoro_();
  ret->ret = qbFuture;
  ret->main = coro_new(entry);
  return ret;
}

qbResult qb_coro_destroy(qbCoro* coro) {
  coro_free((*coro)->main);
  delete *coro;
  *coro = nullptr;
  return QB_OK;
}

qbVar qb_coro_call(qbCoro coro, qbVar var) {
  coro->arg = var;
  return coro_call(coro->main, var);
}

qbCoro qb_coro_sync(qbVar(*entry)(qbVar), qbVar var) {
  return coro_scheduler->schedule_sync(entry, var);
}

qbCoro qb_coro_async(qbVar(*entry)(qbVar), qbVar var) {
  return coro_scheduler->schedule_async(entry, var);
}

qbVar qb_coro_await(qbCoro coro) {
  return coro_scheduler->await(coro);
}

qbVar qb_coro_peek(qbCoro coro) {
  return coro_scheduler->peek(coro);
}

qbVar qb_coro_yield(qbVar var) {
  return coro_yield(var);
}

void qb_coro_wait(double seconds) {
  double start = (double)qb_timer_query() / 1e9;
  double end = start + seconds;
  while ((double)qb_timer_query() / 1e9 < end) {
    qb_coro_yield(qbFuture);
  }
}

void qb_coro_waitframes(uint32_t frames) {
  uint32_t frames_waited = 0;
  while (frames_waited < frames) {
    qb_coro_yield(qbFuture);
    ++frames_waited;
  }
}

bool qb_coro_done(qbCoro coro) {
  return qb_coro_peek(coro).tag != QB_TAG_UNSET;
}

qbVar qbVoid(void* p) {
  qbVar v;
  v.tag = QB_TAG_VOID;
  v.p = p;
  return v;
}

qbVar qbUint(uint64_t u) {
  qbVar v;
  v.tag = QB_TAG_UINT;
  v.u = u;
  return v;
}

qbVar qbInt(int64_t i) {
  qbVar v;
  v.tag = QB_TAG_INT;
  v.i = i;
  return v;
}

qbVar qbDouble(double d) {
  qbVar v;
  v.tag = QB_TAG_DOUBLE;
  v.d = d;
  return v;
}

qbVar qbChar(char c) {
  qbVar v;
  v.tag = QB_TAG_CHAR;
  v.c = c;
  return v;
}

qbResult qb_scene_create(qbScene* scene, const char* name) {
  return AS_PRIVATE(scene_create(scene, name));
}

qbResult qb_scene_destroy(qbScene* scene) {
  return AS_PRIVATE(scene_destroy(scene));
}

qbScene qb_scene_global() {
  return AS_PRIVATE(scene_global());
}

qbResult qb_scene_save(qbScene* scene, const char* file) {
  return QB_OK;
}

qbResult qb_scene_load(qbScene* scene, const char* name, const char* file) {
  return QB_OK;
}

qbResult qb_scene_set(qbScene scene) {
  return AS_PRIVATE(scene_set(scene));
}

const char* qb_scene_name(qbScene scene) {
  return scene->name;
}

qbResult qb_scene_reset() {
  return AS_PRIVATE(scene_reset());
}

qbResult qb_scene_activate(qbScene scene) {
  return AS_PRIVATE(scene_activate(scene));
}

qbResult qb_scene_ondestroy(qbScene scene, void(*fn)(qbScene scene)) {
  return AS_PRIVATE(scene_ondestroy(scene, fn));
}

qbResult qb_scene_onactivate(qbScene scene, void(*fn)(qbScene scene)) {
  return AS_PRIVATE(scene_onactivate(scene, fn));
}

qbResult qb_scene_ondeactivate(qbScene scene, void(*fn)(qbScene scene)) {
  return AS_PRIVATE(scene_ondeactivate(scene, fn));
}