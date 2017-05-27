#include "cubez.h"
#include "private_universe.h"

#define AS_PRIVATE(expr) ((PrivateUniverse*)(universe_->self))->expr

namespace cubez {


static Universe* universe_ = nullptr;

Universe* universe() {
  return universe_;
}

Status::Code init(Universe* u) {
  universe_ = u;
  universe_->self = new PrivateUniverse();

  return AS_PRIVATE(init());
}

Status::Code start() {
  return AS_PRIVATE(start());
}

Status::Code stop() {
  Status::Code ret = AS_PRIVATE(stop());
  universe_ = nullptr;
  return ret;
}

Status::Code loop() {
  return AS_PRIVATE(loop());
}

Id create_program(const char* name) {
  return AS_PRIVATE(create_program(name));
}

Status::Code run_program(Id program) {
  return AS_PRIVATE(run_program(program));
}

struct Pipeline* add_pipeline(const char* program, const char* source, const char* sink) {
  return AS_PRIVATE(add_pipeline(program, source, sink));
}

struct Pipeline* copy_pipeline(struct Pipeline* pipeline, const char* dest) {
  return AS_PRIVATE(copy_pipeline(pipeline, dest));
}

Status::Code remove_pipeline(struct Pipeline* pipeline) {
  return AS_PRIVATE(remove_pipeline(pipeline));
}

Status::Code enable_pipeline(struct Pipeline* pipeline, ExecutionPolicy policy) {
  return AS_PRIVATE(enable_pipeline(pipeline, policy));
}

Status::Code disable_pipeline(struct Pipeline* pipeline) {
  return AS_PRIVATE(disable_pipeline(pipeline));
}

Collection* add_collection(const char* program, const char* name) {
  return AS_PRIVATE(add_collection(program, name));
}

Status::Code add_source(Pipeline* pipeline, const char* source) {
  return AS_PRIVATE(add_source(pipeline, source));
}

Status::Code add_sink(Pipeline* pipeline, const char* sink) {
  return AS_PRIVATE(add_sink(pipeline, sink));
}

Status::Code share_collection(const char* source, const char* dest) {
  return AS_PRIVATE(share_collection(source, dest));
}

Status::Code copy_collection(const char* source, const char* dest) {
  return AS_PRIVATE(copy_collection(source, dest));
}

Arg* get_arg(Frame* frame, const char* name) {
  return FrameImpl::get_arg(frame, name);
}

Arg* new_arg(Frame* frame, const char* name, size_t size) {
  return FrameImpl::new_arg(frame, name, size);
}

Arg* set_arg(Frame* frame, const char* name, void* data, size_t size) {
  return FrameImpl::set_arg(frame, name, data, size);
}

Id create_event(const char* program, const char* event, EventPolicy policy) { 
  return AS_PRIVATE(create_event(program, event, policy));
}

Status::Code flush_events(const char* program, const char* event) {
  return AS_PRIVATE(flush_events(program, event));
}

Message* new_message(struct Channel* c) {
  return ChannelImpl::to_impl(c)->new_message();
}

void send_message(Message* m) {
  ChannelImpl::to_impl(m->channel)->send_message(m);
}

struct Channel* open_channel(const char* program, const char* event) {
  return AS_PRIVATE(open_channel(program, event));
}

void close_channel(struct Channel* channel) {
  AS_PRIVATE(close_channel(channel));
}

struct Subscription* subscribe_to(
    const char* program, const char* event, struct Pipeline* pipeline) {
  return AS_PRIVATE(subscribe_to(program, event, pipeline));
}

void unsubscribe_from(struct Subscription* subscription) {
  AS_PRIVATE(unsubscribe_from(subscription));
}


}  // namespace cubez
