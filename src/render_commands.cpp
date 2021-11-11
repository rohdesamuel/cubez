#include "render_commands.h"

#include <cubez/memory.h>
#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include "render_pipeline_defs.h"
#include "draw_command_buffer.h"

#include <functional>
#include <vector>

namespace
{
#include "gl_translate_utils.h"
}

typedef void (*RenderCommandFn)(qbDrawState state, qbRenderCommand c);

void rendercmd_noop(qbDrawState state, qbRenderCommand c) {}

void rendercmd_setcull(qbDrawState state, qbRenderCommand c) {
  qbFace cull_face = c->command.set_cull.cull_face;
  if (cull_face == QB_FACE_NONE) {
    glDisable(GL_CULL_FACE);
  } else {
    glEnable(GL_CULL_FACE);
    glCullFace(TranslateQbCullFaceToOpenGl(cull_face));
  }
}

void rendercmd_beginpass(qbDrawState state, qbRenderCommand c) {
  qbFrameBuffer framebuffer = state->framebuffer;
  qbRenderPass render_pass = state->render_pass;

  if (!framebuffer) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return;
  }

  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->id);
  if (state->clear_values_count == 0) {
    return;
  }

  for (size_t i = 0; i < render_pass->attachments.size(); ++i) {
    qbFramebufferAttachmentRef ref = &render_pass->attachments[i];
    if (ref->attachment == QB_UNUSED_FRAMEBUFFER_ATTACHMENT) continue;

    qbFramebufferAttachment attachment = &framebuffer->attachments[i];
    qbClearValue clear_value = &state->clear_values[i];

    switch (ref->aspect) {
      case QB_COLOR_ASPECT:
        if (TranslateQbPixelFormatToOpenGlSize(attachment->image->format) == GL_UNSIGNED_BYTE) {
          glClearBufferuiv(GL_COLOR, ref->attachment, clear_value->color.uint32);
        } else {
          glClearBufferfv(GL_COLOR, ref->attachment, clear_value->color.float32);
        }
        CHECK_GL();
        break;
      case QB_DEPTH_ASPECT:
        glClearBufferfv(GL_DEPTH, 0, &clear_value->depth);
        CHECK_GL();
        break;
      case QB_STENCIL_ASPECT:
        glClearBufferiv(GL_DEPTH, 0, (int32_t*)&clear_value->stencil);
        CHECK_GL();
        break;
      case QB_DEPTH_STENCIL_ASPECT:
        glClearBufferfi(GL_DEPTH_STENCIL, 0, clear_value->depth, (int32_t)clear_value->stencil);
        CHECK_GL();
        break;
    }
  }

  CHECK_GL();
}

void rendercmd_endpass(qbDrawState state, qbRenderCommand c) {
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_DEPTH_CLAMP);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_BLEND);

  CHECK_GL();
}

void rendercmd_setviewport(qbDrawState state, qbRenderCommand c) {
  qbViewport_& view = c->command.set_viewport.viewport;

  glViewport((GLint)view.x, (GLint)view.y, (GLsizei)view.w, (GLsizei)view.h);
  glDepthRange((GLclampd)view.min_depth, (GLclampd)view.max_depth);

  CHECK_GL();
}

void rendercmd_setscissor(qbDrawState state, qbRenderCommand c) {
  qbRect_& r = c->command.set_scissor.rect;
  glEnable(GL_SCISSOR_TEST);
  glScissor((GLint)r.x, (GLint)r.y, (GLsizei)r.w, (GLsizei)r.h);

  CHECK_GL();
}

void rendercmd_bindpipeline(qbDrawState state, qbRenderCommand c) {
  qbRenderPipeline pipeline = c->command.bind_pipeline.pipeline;
  state->pipeline = pipeline;

  if (!c->command.bind_pipeline.set_render_state) {
    return;
  }

  qbRasterizationInfo raster_info = pipeline->rasterization_info;

  qbViewportState view = pipeline->viewport_state;
  glViewport((GLint)view->viewport.x, (GLint)view->viewport.y, (GLsizei)view->viewport.w, (GLsizei)view->viewport.h);

  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_DEPTH_CLAMP);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_BLEND);

  glEnable(GL_SCISSOR_TEST);
  glScissor((GLint)view->scissor.x, (GLint)view->scissor.y, (GLsizei)view->scissor.w, (GLsizei)view->scissor.h);

  glFrontFace(TranslateQbFrontFaceToOpenGl(raster_info->front_face));

  if (raster_info->cull_face != QB_FACE_NONE) {
    glEnable(GL_CULL_FACE);
    glCullFace(TranslateQbCullFaceToOpenGl(raster_info->cull_face));
  }

  qbDepthStencilState depth_stencil_state = raster_info->depth_stencil_state;
  if (depth_stencil_state->depth_test_enable) {
    glEnable(GL_DEPTH_TEST);
    if (raster_info->enable_depth_clamp) {
      glEnable(GL_DEPTH_CLAMP);
    }

    glDepthRange(
      (float)pipeline->viewport_state->viewport.min_depth,
      (float)pipeline->viewport_state->viewport.max_depth);
    glDepthFunc(TranslateQbRenderTestFuncToOpenGl(depth_stencil_state->depth_compare_op));
    glDepthMask(depth_stencil_state->depth_write_enable);
  }

  if (depth_stencil_state->stencil_test_enable) {
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(TranslateQbRenderTestFuncToOpenGl(depth_stencil_state->stencil_compare_op),
                  depth_stencil_state->stencil_ref_value,
                  depth_stencil_state->stencil_mask);
  }

  qbColorBlendState blend_state = pipeline->blend_state;
  if (blend_state->blend_enable) {
    glEnable(GL_BLEND);

    glBlendEquationSeparate(
      TranslateQbBlendEquationToOpenGl(blend_state->rgb_blend.op),
      TranslateQbBlendEquationToOpenGl(blend_state->alpha_blend.op));

    glBlendFuncSeparate(
      TranslateQbBlendFactorToOpenGl(blend_state->rgb_blend.src),
      TranslateQbBlendFactorToOpenGl(blend_state->rgb_blend.dst),
      TranslateQbBlendFactorToOpenGl(blend_state->alpha_blend.src),
      TranslateQbBlendFactorToOpenGl(blend_state->alpha_blend.dst));

    glBlendColor(blend_state->blend_color.x,
                  blend_state->blend_color.y,
                  blend_state->blend_color.z,
                  blend_state->blend_color.w);
  }

  glPolygonMode(TranslateQbCullFaceToOpenGl(raster_info->raster_face),
                TranslateQbPolygonModeToOpenGl(raster_info->raster_mode));
  glPointSize(raster_info->point_size);
  glLineWidth(raster_info->line_width);

  {
    glBindVertexArray(pipeline->render_vao);
    qbGeometryDescriptor geometry = pipeline->geometry;
    for (size_t i = 0; i < geometry->attributes_count; ++i) {
      glEnableVertexAttribArray(geometry->attributes[i].location);
    }
  }

  pipeline->shader_module->shader->use();

  CHECK_GL();
}

void bind_shaderresourcesets(qbDrawState state, qbShaderResourcePipelineLayout layout, uint32_t resource_set_count, qbShaderResourceSet* resource_sets) {
  qbRenderPipeline pipeline = state->pipeline;
  ShaderProgram& shader = *pipeline->shader_module->shader;

  uint32_t program = (uint32_t)shader.id();

  for (size_t i = 0; i < resource_set_count; ++i) {
    qbShaderResourceSet resource_set = resource_sets[i];

    for (size_t j = 0; j < resource_set->bindings.size(); ++j) {
      qbShaderResourceBinding binding = resource_set->bindings[j];

      switch (binding->resource_type) {
        case QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER: {
          if (binding->name && !GLEW_ARB_shading_language_420pack) {
            int32_t block_index = glGetUniformBlockIndex(program, binding->name);
            glUniformBlockBinding(program, block_index, binding->binding);
          }
          glBindBufferBase(GL_UNIFORM_BUFFER, binding->binding, resource_set->uniforms[j]->id);
          CHECK_GL();
          break;
        }

        case QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER: {
          qbImage image = resource_set->images[j];
          qbImageSampler sampler = resource_set->samplers[j];
          uint32_t texture_slot = binding->binding;//shader.texture_slot(binding->name);

          glActiveTexture((GLenum)(GL_TEXTURE0 + texture_slot));

          //if (binding->name && !GLEW_ARB_shading_language_420pack) {
          glUniform1i(glGetUniformLocation(program, binding->name), (GLint)texture_slot);
          //}

          glBindSampler((GLuint)texture_slot, (GLuint)sampler->id);
          glBindTexture(TranslateQbImageTypeToOpenGl(image->type), (GLuint)image->id);

          CHECK_GL();
          break;
        }
      }
    }
  }
}

void rendercmd_bindshaderresourceset(qbDrawState state, qbRenderCommand c) {
  qbShaderResourcePipelineLayout layout = c->command.bind_shaderresourceset.layout;
  qbShaderResourceSet resource_set = c->command.bind_shaderresourceset.resource_set;

  qbShaderResourceSet* resource_sets = (qbShaderResourceSet*)&resource_set;
  bind_shaderresourcesets(state, layout, 1, resource_sets);
}

void rendercmd_bindshaderresourcesets(qbDrawState state, qbRenderCommand c) {
  qbShaderResourcePipelineLayout layout = c->command.bind_shaderresourcesets.layout;
  uint32_t resource_set_count = c->command.bind_shaderresourcesets.resource_set_count;
  qbShaderResourceSet* resource_sets = c->command.bind_shaderresourcesets.resource_sets;

  bind_shaderresourcesets(state, layout, resource_set_count, resource_sets);
}

void rendercmd_bindvertexbuffers(qbDrawState state, qbRenderCommand c) {
  uint32_t first_binding = c->command.bind_vertexbuffers.first_binding;
  uint32_t binding_count = c->command.bind_vertexbuffers.binding_count;
  qbGpuBuffer* buffers = c->command.bind_vertexbuffers.buffers;

  qbGeometryDescriptor geometry = state->pipeline->geometry;

  for (uint32_t count = 0; count < binding_count; ++count) {
    uint32_t binding = first_binding + count;
    qbGpuBuffer buffer = buffers[count];

    GLenum target = TranslateQbGpuBufferTypeToOpenGl(buffer->buffer_type);
    glBindBuffer(target, buffer->id);

    qbBufferBinding buffer_binding = nullptr;
    for (size_t j = 0; j < geometry->bindings_count; ++j) {
      buffer_binding = geometry->bindings + j;
      if (geometry->bindings[j].binding == binding) {
        break;
      }
    }
    assert(buffer_binding && "Could not find a qbBufferBinding.");

    qbVertexAttribute vertex_attribute = nullptr;
    for (size_t j = 0; j < geometry->attributes_count; ++j) {
      vertex_attribute = geometry->attributes + j;
      if (vertex_attribute->binding == binding) {
        glVertexAttribPointer(vertex_attribute->location,
                              (GLint)vertex_attribute->count,
                              TranslateQbVertexAttribTypeToOpenGl(vertex_attribute->type),
                              vertex_attribute->normalized,
                              buffer_binding->stride,
                              vertex_attribute->offset);
      }
    }
  }

  CHECK_GL();
}


void rendercmd_bindindexbuffer(qbDrawState state, qbRenderCommand c) {
  qbGpuBuffer buffer = c->command.bind_indexbuffer.buffer;
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer->id);

  CHECK_GL();
}

void rendercmd_draw(qbDrawState state, qbRenderCommand c) {
  uint32_t vertex_count = c->command.draw.vertex_count;
  uint32_t instance_count = c->command.draw.instance_count;
  uint32_t first_vertex = c->command.draw.first_vertex;
  uint32_t first_instance = c->command.draw.first_instance;

  GLenum draw_mode = TranslateQbDrawModeToOpenGlMode(state->pipeline->geometry->mode);

  if (instance_count == 0) {
    glDrawArrays(draw_mode, first_vertex, (GLsizei)vertex_count);
  } else {
    glDrawArraysInstanced(draw_mode, first_instance, (GLsizei)vertex_count, instance_count);
  }

  CHECK_GL();
}

void rendercmd_drawindexed(qbDrawState state, qbRenderCommand c) {
  uint32_t index_count = c->command.draw_indexed.index_count;
  uint32_t instance_count = c->command.draw_indexed.instance_count;
  uint32_t vertex_offset = c->command.draw_indexed.vertex_offset;
  uint32_t first_instance = c->command.draw_indexed.first_instance;

  GLenum draw_mode = TranslateQbDrawModeToOpenGlMode(state->pipeline->geometry->mode);
  if (instance_count == 0) {
    glDrawElementsBaseVertex(draw_mode, (GLsizei)index_count, GL_UNSIGNED_INT, nullptr, vertex_offset);
  } else {
    glDrawElementsInstancedBaseVertex(draw_mode, (GLsizei)index_count, GL_UNSIGNED_INT, nullptr, (GLsizei)instance_count, first_instance);
  }

  CHECK_GL();
}

void rendercmd_updatebuffer(qbDrawState state, qbRenderCommand c) {
  qbGpuBuffer buffer = c->command.update_buffer.buffer;
  intptr_t offset = c->command.update_buffer.offset;
  size_t size = c->command.update_buffer.size;
  void* data = c->command.update_buffer.data;

  qb_gpubuffer_update(buffer, offset, size, data);
}

void rendercmd_subcommands(qbDrawState state, qbRenderCommand c) {
  qbDrawCommandBuffer buf = c->command.sub_commands.cmd_buf;
  buf->execute();
}

// WARNING: The order of the jump table must match the enums in `qbRenderCommandType_`.
RenderCommandFn render_command_jump_table[] = {
  rendercmd_noop,
  rendercmd_beginpass,
  rendercmd_endpass,
  rendercmd_bindpipeline,
  rendercmd_setcull,
  rendercmd_setviewport,
  rendercmd_setscissor,
  rendercmd_bindshaderresourceset,
  rendercmd_bindshaderresourcesets,
  rendercmd_bindvertexbuffers,
  rendercmd_bindindexbuffer,
  rendercmd_draw,
  rendercmd_drawindexed,
  rendercmd_updatebuffer,
  rendercmd_subcommands
};

void RenderCommandQueue::clear() {
  state = {};
  qb_taskbundle_clear(task_bundle);
  allocator->clear(allocator);
  queued_commands.resize(0);
}

void RenderCommandQueue::execute() {
  for (auto& c : queued_commands) {
    (*render_command_jump_table[c.type])(&state, &c);
  }
}

qbDrawCommandBuffer_::qbDrawCommandBuffer_(qbMemoryAllocator allocator) : allocator_(allocator) {
  // TODO: Change this to an object pool.
  state_allocator_ = qb_memallocator_default();
}

qbTask qbDrawCommandBuffer_::submit(qbDrawCommandSubmitInfo submit_info) {
  for (RenderCommandQueue* queue : queued_passes_) {
    queue->execute();
  }
  return qbInvalidHandle;
}

void qbDrawCommandBuffer_::execute() {
  for (RenderCommandQueue* queue : queued_passes_) {
    queue->execute();
  }
}

void qbDrawCommandBuffer_::clear() {
  for (RenderCommandQueue* queue : queued_passes_) {
    queue->clear();

    return_bundle(queue->task_bundle);
    return_allocator(queue->allocator);
    return_queue(queue);
  }

  queued_passes_.clear();
  builder_ = nullptr;
  if (allocator_->clear) {
    allocator_->clear(allocator_);
  }
}

void qbDrawCommandBuffer_::begin_pass(qbBeginRenderPassInfo begin_info) {
  builder_ = allocate_queue();

  builder_->task_bundle = allocate_bundle();
  builder_->allocator = allocate_allocator();  
  builder_->state.clear_values_count = begin_info->clear_values_count;

  if (builder_->state.clear_values_count > 0) {
    builder_->state.clear_values = allocate<qbClearValue_>(builder_->state.clear_values_count);
    std::copy(
      begin_info->clear_values,
      begin_info->clear_values + builder_->state.clear_values_count,
      builder_->state.clear_values);
  } else {
    builder_->state.clear_values = nullptr;
  }

  builder_->state.framebuffer = begin_info->framebuffer;
  builder_->state.render_pass = begin_info->render_pass;

  //qb_taskbundle_begin(builder_->task_bundle, {});
}

void qbDrawCommandBuffer_::end_pass() {
  //qb_taskbundle_end(builder_->task_bundle);
  queue_pass();
}

void qbDrawCommandBuffer_::queue_command(qbRenderCommand command) {
  builder_->queued_commands.push_back(*command);
}

void qbDrawCommandBuffer_::queue_pass() {
  queued_passes_.push_back(builder_);
  builder_ = nullptr;
}

RenderCommandQueue* qbDrawCommandBuffer_::allocate_queue() {
  if (allocated_queues_.empty()) {
    return new RenderCommandQueue{};
  }
  RenderCommandQueue* ret = allocated_queues_.back();
  allocated_queues_.pop_back();

  return ret;
}

qbTaskBundle qbDrawCommandBuffer_::allocate_bundle() {
  if (allocated_bundles_.empty()) {
    return qb_taskbundle_create({});
  }
  qbTaskBundle ret = allocated_bundles_.back();
  allocated_bundles_.pop_back();

  return ret;
}

qbMemoryAllocator qbDrawCommandBuffer_::allocate_allocator() {
  if (allocated_allocators_.empty()) {
    return qb_memallocator_paged(4096);
  }
  qbMemoryAllocator ret = allocated_allocators_.back();
  allocated_allocators_.pop_back();

  return ret;
}

void qbDrawCommandBuffer_::return_bundle(qbTaskBundle task_bundle) {
  allocated_bundles_.push_back(task_bundle);
}

void qbDrawCommandBuffer_::return_allocator(qbMemoryAllocator allocator) {
  allocated_allocators_.push_back(allocator);
}

void qbDrawCommandBuffer_::return_queue(RenderCommandQueue* queue) {
  allocated_queues_.push_back(queue);
}