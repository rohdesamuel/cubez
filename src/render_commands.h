/**
* Author: Samuel Rohde (rohde.samuel@cubez.io)
*
* Copyright 2021 Samuel Rohde
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#ifndef CUBEZ_RENDER_COMMANDS__H
#define CUBEZ_RENDER_COMMANDS__H

#include <cubez/common.h>
#include <cubez/memory.h>
#include "render_pipeline_defs.h"

typedef struct qbRenderCommands_* qbRenderCommands;
typedef enum qbRenderCommandType_ {
  QB_RENDER_COMMAND_NOOP,
  QB_RENDER_COMMAND_BEGIN,
  QB_RENDER_COMMAND_END,
  QB_RENDER_COMMAND_BINDPIPELINE,
  QB_RENDER_COMMAND_BEGINPIPELINE,
  QB_RENDER_COMMAND_SETCULL,
  QB_RENDER_COMMAND_SETVIEWPORT,
  QB_RENDER_COMMAND_SETSCISSOR,
  QB_RENDER_COMMAND_UPDATESHADERRESOURCE,
  QB_RENDER_COMMAND_UPDATESHADERRESOURCES,
  QB_RENDER_COMMAND_BINDSHADERRESOURCESET,
  QB_RENDER_COMMAND_BINDSHADERRESOURCESETS,
  QB_RENDER_COMMAND_BINDVERTEXBUFFERS,
  QB_RENDER_COMMAND_BINDINDEXBUFFER,
  QB_RENDER_COMMAND_DRAW,
  QB_RENDER_COMMAND_DRAWINDEXED,
  QB_RENDER_COMMAND_UPDATEBUFFER,
  QB_RENDER_COMMAND_SUBCOMMANDS,
  QB_RENDER_COMMAND_REFCOMMANDS,
  QB_RENDER_COMMAND_SIGNAL,
  QB_RENDER_COMMAND_WAIT,
  QB_RENDER_COMMAND_RESETSIGNAL,
} qbRenderCommandType_;

typedef struct qbRenderCommandBegin_ {
  uint64_t reserved;
} qbRenderCommandBegin_;

typedef struct qbRenderCommandEnd_ {
  uint64_t reserved;
} qbRenderCommandEnd_;

typedef struct qbRenderCommandBindPipeline_ {
  struct qbRenderPipeline_* pipeline;
} qbRenderCommandBindPipeline_;

typedef struct qbRenderCommandBeginPipeline_ {
  struct qbRenderPipeline_* pipeline;
} qbRenderCommandBeginPipeline_;

typedef struct qbRenderCommandSetCull_ {
  qbFace cull_face;
} qbRenderCommandSetCull_;

typedef struct qbRenderCommandSetViewport_ {
  qbViewport_ viewport;
} qbRenderCommandSetViewport_;

typedef struct qbRenderCommandSetScissor_ {
  qbRect_ rect;
} qbRenderCommandSetScissor_;

typedef struct qbRenderCommandUpdateShaderResource_ {
  uint32_t binding;
  qbImage image;
  qbGpuBuffer buffer;
  qbShaderResourceSet resource_set;
} qbRenderCommandUpdateShaderResource_;

typedef struct qbRenderCommandUpdateShaderResources_{
  size_t binding_count;
  uint32_t* bindings;
  qbImage* images;
  qbGpuBuffer* buffers;
  qbShaderResourceSet resource_set;
} qbRenderCommandUpdateShaderResources_;

typedef struct qbRenderCommandBindShaderResourceSet_ {
  qbShaderResourceSet resource_set;
} qbRenderCommandBindShaderResourceSet_;

typedef struct qbRenderCommandBindShaderResourceSets_ {
  uint32_t resource_set_count;
  qbShaderResourceSet* resource_sets;
} qbRenderCommandBindShaderResourceSets_;

typedef struct qbRenderCommandBindVertexBuffers_ {
  uint32_t first_binding;
  uint32_t binding_count;
  qbGpuBuffer* buffers;
} qbRenderCommandBindVertexBuffers_;

typedef struct qbRenderCommandBindIndexBuffer_ {
  qbGpuBuffer buffer;
} qbRenderCommandBindIndexBuffer_;

typedef struct qbRenderCommandDraw_ {
  uint32_t vertex_count;
  uint32_t instance_count;
  uint32_t first_vertex;
  uint32_t first_instance;
} qbRenderCommandDraw_;

typedef struct qbRenderCommandDrawIndexed_ {
  uint32_t index_count;
  uint32_t instance_count;
  uint32_t vertex_offset;
  uint32_t first_instance;
} qbRenderCommandDrawIndexed_;

typedef struct qbRenderCommandUpdateBuffer_ {
  qbGpuBuffer buffer;
  intptr_t offset;
  size_t size;
  void* data;
} qbRenderCommandUpdateBuffer_;

typedef struct qbRenderCommandSubCommands_ {
  qbDrawCommandBuffer cmd_buf;
} qbRenderCommandSubCommands_;

typedef struct qbRenderCommandRefCommands_ {
  qbDrawCommandBuffer* cmd_buf;
  qbSemaphore opt_sem;
  uint64_t wait_n;
} qbRenderCommandRefCommands_;

typedef struct qbRenderCommandSignal_ {
  qbSemaphore semaphore;
  uint64_t n;
} qbRenderCommandSignal_;

typedef struct qbRenderCommandWait_ {
  qbSemaphore semaphore;
  uint64_t n;
} qbRenderCommandWait_;

typedef struct qbRenderCommandResetSignal_ {
  qbSemaphore semaphore;
} qbRenderCommandResetSignal_;

typedef struct qbRenderCommand_ {
  qbRenderCommandType_ type;
  union {
    qbRenderCommandBegin_ begin;
    qbRenderCommandEnd_ end;
    qbRenderCommandBindPipeline_ bind_pipeline;
    qbRenderCommandBeginPipeline_ begin_pipeline;
    qbRenderCommandSetCull_ set_cull;
    qbRenderCommandSetViewport_ set_viewport;
    qbRenderCommandSetScissor_ set_scissor;
    qbRenderCommandUpdateShaderResource_ update_shaderresource;
    qbRenderCommandUpdateShaderResources_ update_shaderresources;
    qbRenderCommandBindShaderResourceSet_ bind_shaderresourceset;
    qbRenderCommandBindShaderResourceSets_ bind_shaderresourcesets;
    qbRenderCommandBindVertexBuffers_ bind_vertexbuffers;
    qbRenderCommandBindIndexBuffer_ bind_indexbuffer;
    qbRenderCommandDraw_ draw;
    qbRenderCommandDrawIndexed_ draw_indexed;
    qbRenderCommandUpdateBuffer_ update_buffer;
    qbRenderCommandSubCommands_ sub_commands;
    qbRenderCommandRefCommands_ ref_commands;
    qbRenderCommandSignal_ signal;
    qbRenderCommandWait_ wait;
    qbRenderCommandResetSignal_ reset_signal;
  } command;

} qbRenderCommand_, *qbRenderCommand;


#endif  // CUBEZ_RENDER_COMMANDS__H