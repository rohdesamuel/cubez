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

}  // namespace cubez
