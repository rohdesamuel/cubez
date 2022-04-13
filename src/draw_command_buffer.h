#ifndef DRAW_COMMAND_BUFFER__H
#define DRAW_COMMAND_BUFFER__H

#include <cubez/common.h>
#include <cubez/memory.h>

#include "render_pipeline_defs.h"

#include <vector>

typedef struct qbDrawState_ {
  qbRenderPipeline pipeline = nullptr;
  qbRenderPass render_pass = nullptr;
  qbFrameBuffer framebuffer = nullptr;
  qbClearValue clear_values = nullptr;
  uint32_t clear_values_count = 0;
} qbDrawState_, * qbDrawState;

struct RenderCommandQueue {
  qbDrawState_ state = {};
  qbTaskBundle task_bundle = nullptr;
  qbMemoryAllocator allocator = nullptr;
  std::vector<qbRenderCommand_> queued_commands = {};

  void clear();
  void execute();
};

struct qbDrawCommandBuffer_ {
  qbDrawCommandBuffer_(qbMemoryAllocator allocator);

  qbTask submit(qbDrawCommandSubmitInfo submit_info);

  // Executes /all/ commands and clears the queued commands.
  qbTask flush();
  void execute();

  void clear();
  void begin_pass(qbBeginRenderPassInfo begin_info);
  void end_pass();
  void queue_command(qbRenderCommand command);

  template<class Ty_>
  Ty_* allocate(size_t count = 1) {
    qbMemoryAllocator allocator = builder_->allocator;
    return (Ty_*)qb_alloc(allocator, sizeof(Ty_) * count);
  }

  template<>
  void* allocate<void>(size_t count) {
    qbMemoryAllocator allocator = builder_->allocator;
    return qb_alloc(allocator, count);
  }

private:
  RenderCommandQueue* builder_ = nullptr;
  std::vector<RenderCommandQueue*> queued_passes_ = {};
  std::vector<qbTaskBundle> allocated_bundles_ = {};
  std::vector<qbMemoryAllocator> allocated_allocators_ = {};
  std::vector<RenderCommandQueue*> allocated_queues_ = {};

  qbMemoryAllocator allocator_ = nullptr;
  qbMemoryAllocator state_allocator_ = nullptr;

  void queue_pass();

  RenderCommandQueue* allocate_queue();
  qbTaskBundle allocate_bundle();
  qbMemoryAllocator allocate_allocator();

  void return_bundle(qbTaskBundle task_bundle);
  void return_allocator(qbMemoryAllocator allocator);
  void return_queue(RenderCommandQueue* queue);

};

#endif  // DRAW_COMMAND_BUFFER__H