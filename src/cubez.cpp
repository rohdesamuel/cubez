#include "cubez.h"
#include "private_universe.h"

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

struct qbSystem* qb_alloc_system(const char* program, const char* source, const char* sink) {
  return AS_PRIVATE(alloc_system(program, source, sink));
}

struct qbSystem* qb_copy_system(struct qbSystem* system, const char* dest) {
  return AS_PRIVATE(copy_system(system, dest));
}

qbResult qb_free_system(struct qbSystem* system) {
  return AS_PRIVATE(free_system(system));
}

qbResult qb_enable_system(struct qbSystem* system, qbExecutionPolicy policy) {
  return AS_PRIVATE(enable_system(system, policy));
}

qbResult qb_disable_system(struct qbSystem* system) {
  return AS_PRIVATE(disable_system(system));
}

qbCollection* qb_alloc_collection(const char* program, const char* name) {
  return AS_PRIVATE(alloc_collection(program, name));
}

qbResult qb_add_source(qbSystem* system, const char* source) {
  return AS_PRIVATE(add_source(system, source));
}

qbResult qb_share_collection(
      const char* source_program, const char* source_collection,
      const char* dest_program, const char* dest_collection) {
  return AS_PRIVATE(share_collection(source_program, source_collection,
                                     dest_program, dest_collection));
}

qbResult qb_copy_collection(
      const char* source_program, const char* source_collection,
      const char* dest_program, const char* dest_collection) {
  return AS_PRIVATE(copy_collection(source_program, source_collection,
                                    dest_program, dest_collection));
}

qbId qb_create_event(const char* program, const char* event, qbEventPolicy policy) { 
  return AS_PRIVATE(create_event(program, event, policy));
}

qbResult qb_flush_events(const char* program, const char* event) {
  return AS_PRIVATE(flush_events(program, event));
}

struct qbMessage* qb_alloc_message(struct qbChannel* c) {
  return ChannelImpl::to_impl(c)->alloc_message();
}

void qb_send_message(qbMessage* m) {
  ChannelImpl::to_impl(m->channel)->send_message(m);
}

void qb_send_message_sync(qbMessage* m) {
  ChannelImpl::to_impl(m->channel)->send_message_sync(m);
}

struct qbChannel* qb_open_channel(const char* program, const char* event) {
  return AS_PRIVATE(open_channel(program, event));
}

void qb_close_channel(struct qbChannel* channel) {
  AS_PRIVATE(close_channel(channel));
}

struct qbSubscription* qb_subscribe_to(
    const char* program, const char* event, struct qbSystem* system) {
  return AS_PRIVATE(subscribe_to(program, event, system));
}

void qb_unsubscribe_from(struct qbSubscription* subscription) {
  AS_PRIVATE(unsubscribe_from(subscription));
}
