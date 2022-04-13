/**
* Author: Samuel Rohde (rohde.samuel@cubez.io)
*
* Copyright 2020 Samuel Rohde
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
// https://sopyer.github.io/Blog/post/minimal-vulkan-sample/

#define GLM_GTC_matrix_transform
#include <cubez/cubez.h>

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <cubez/render_pipeline.h>
#include <cubez/memory.h>
#include <cubez/common.h>
#include <algorithm>
#include <mutex>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <cubez/render.h>
#include <cubez/utils.h>
#include <vector>
#include <assert.h>

#include "async_internal.h"
#include "inline_shaders.h"
#include "render_internal.h"
#include "render_commands.h"
#include "shader.h"
#include "stb_image.h"
#include "render_pipeline_defs.h"
#include "draw_command_buffer.h"
#include "gl_translate_utils.h"

#define WITH_MODULE(module, fn, ...) (module)->render_interface.fn((module)->impl, __VA_ARGS__)


void qb_renderpass_draw(qbRenderPass render_pass, qbFrameBuffer frame_buffer);

qbPixelMap qb_pixelmap_create(uint32_t width, uint32_t height, uint32_t depth,
                              qbPixelFormat format, void* pixels) {
  qbPixelMap p = new qbPixelMap_;
  p->width = width;
  p->height = height;
  p->depth = depth;
  p->format = format;

  p->pixel_size = TranslateQbPixelFormatToPixelSize(format);
  if (height == 0) {
    p->size = width;
  } else if (depth == 0) {
    p->size = width * height;
  } else if (depth > 0) {
    p->size = width * height * depth;
  }
  p->size *= p->pixel_size;
  if (p->size) {
    p->pixels = new uint8_t[p->size];
  } else {
    p->pixels = nullptr;
  }

  p->width_size = p->width * p->pixel_size;
  p->height_size = p->height * p->pixel_size;
  p->depth_size = p->depth * p->pixel_size;

  if (p->pixels && pixels) {
    memcpy(p->pixels, pixels, p->size);
  }

  return p;
}

qbPixelMap qb_pixelmap_load(qbPixelFormat format, const char* file) {
  qbPixelMap ret = new qbPixelMap_{};
  ret->depth = 0;
  ret->format = format;
  ret->pixel_size = TranslateQbPixelFormatToPixelSize(format);


  int num_channels = 0;
  switch (format) {
    case QB_PIXEL_FORMAT_R8:
    case QB_PIXEL_FORMAT_R16F:
    case QB_PIXEL_FORMAT_R32F:
    case QB_PIXEL_FORMAT_D32:
    case QB_PIXEL_FORMAT_S8:
      num_channels = 1;
      break;

    case QB_PIXEL_FORMAT_RG8:
    case QB_PIXEL_FORMAT_RG16F:
    case QB_PIXEL_FORMAT_RG32F:
    case QB_PIXEL_FORMAT_D24_S8:
      num_channels = 2;
      break;

    case QB_PIXEL_FORMAT_RGB8:
    case QB_PIXEL_FORMAT_RGB16F:
    case QB_PIXEL_FORMAT_RGB32F:
      num_channels = 3;
      break;

    case QB_PIXEL_FORMAT_RGBA8:
    case QB_PIXEL_FORMAT_RGBA16F:
    case QB_PIXEL_FORMAT_RGBA32F:
      num_channels = 4;
      break;
  }

  // Load the image from the file into SDL's surface representation
  int w, h, n;
  unsigned char* pixels = stbi_load(file, &w, &h, &n, num_channels);

  ret->width = w;
  ret->height = h;
  ret->size = w * h * ret->pixel_size;
  ret->pixels = new uint8_t[ret->size];
  memcpy(ret->pixels, pixels, ret->size);

  if (!pixels) {
    std::cout << "Could not load texture " << file << ": " << stbi_failure_reason() << std::endl;
    return nullptr;
  }

  stbi_image_free(pixels);

  return ret;
}

void qb_pixelmap_destroy(qbPixelMap* pixels) {
  delete[] (*pixels)->pixels;
  delete *pixels;
  *pixels = nullptr;
}

uint32_t qb_pixelmap_width(qbPixelMap pixels) {
  return pixels->width;
}

uint32_t qb_pixelmap_height(qbPixelMap pixels) {
  return pixels->height;
}

uint32_t qb_pixelmap_depth(qbPixelMap pixels) {
  return pixels->depth;
}

uint8_t* qb_pixelmap_pixels(qbPixelMap pixels) {
  return pixels->pixels;
}

uint8_t* qb_pixelmap_at(qbPixelMap pixels, int32_t x, int32_t y, int32_t z) {
  x = pixels->width == 0 ? 0 : x % pixels->width;
  y = pixels->height == 0 ? 0 : y % pixels->height;
  z = pixels->depth == 0 ? 0 : z % pixels->depth;

  return pixels->pixels + (x * pixels->pixel_size) + (pixels->width_size * y) + (pixels->width_size * pixels->height_size * z);
}

qbResult qb_pixelmap_drawto(qbPixelMap src, qbPixelMap dest, vec4s src_rect, vec4s dest_rect) {
  return QB_OK;
}

size_t qb_pixelmap_size(qbPixelMap pixels) {
  return pixels->size;
}

size_t qb_pixelmap_pixelsize(qbPixelMap pixels) {
  return pixels->pixel_size;
}

qbPixelFormat qb_pixelmap_format(qbPixelMap pixels) {
  return pixels->format;
}

void qb_renderpipeline_create(qbRenderPipeline* pipeline, qbRenderPipelineAttr pipeline_attr) {
  *pipeline = new qbRenderPipeline_;
  (*pipeline)->ext = pipeline_attr->ext;

  (*pipeline)->shader_module = pipeline_attr->shader;
  (*pipeline)->render_pass = pipeline_attr->render_pass;
  (*pipeline)->blend_state = new qbColorBlendState_(*pipeline_attr->blend_state);
  
  {
    qbRasterizationInfo copy = new qbRasterizationInfo_;
    *copy = *pipeline_attr->rasterization_info;
    copy->depth_stencil_state = new qbDepthStencilState_;
    *copy->depth_stencil_state = *pipeline_attr->rasterization_info->depth_stencil_state;
    
    if (copy->line_width == 0.f) {
      copy->line_width = 1.f;
    }

    if (copy->point_size == 0.f) {
      copy->point_size = 1.f;
    }

    (*pipeline)->rasterization_info = copy;
  }

  {
    qbGeometryDescriptor copy = new qbGeometryDescriptor_;
    copy->attributes_count = pipeline_attr->geometry->attributes_count;
    copy->attributes = new qbVertexAttribute_[copy->attributes_count];
    std::copy(pipeline_attr->geometry->attributes,
              pipeline_attr->geometry->attributes + copy->attributes_count,
              copy->attributes);

    copy->bindings_count = pipeline_attr->geometry->bindings_count;
    copy->bindings = new qbBufferBinding_[copy->bindings_count];
    std::copy(pipeline_attr->geometry->bindings,
              pipeline_attr->geometry->bindings + copy->bindings_count,
              copy->bindings);

    copy->mode = pipeline_attr->geometry->mode;

    (*pipeline)->geometry = copy;
  }

  {
    qbViewportState copy = new qbViewportState_;
    copy->viewport = pipeline_attr->viewport_state->viewport;
    copy->scissor = pipeline_attr->viewport_state->scissor;
    (*pipeline)->viewport_state = copy;
  }

  (*pipeline)->pipeline_layout = pipeline_attr->resource_layout;

  glGenVertexArrays(1, &(*pipeline)->render_vao);
}

void qb_renderpipeline_destroy(qbRenderPipeline* pipeline) {
  qb_renderext_destroy(&(*pipeline)->ext);
  delete *pipeline;
  *pipeline = nullptr;
}


void qb_shadermodule_create(qbShaderModule* shader_ref, qbShaderModuleAttr attr) {
  qbShaderModule module = *shader_ref = new qbShaderModule_();
  std::string vs = attr->vs ? attr->vs : "";
  std::string fs = attr->fs ? attr->fs : "";
  std::string gs = attr->gs ? attr->gs : "";

  if (attr->interpret_as_strings) {
    if (gs.empty()) {
      module->shader = new ShaderProgram(vs, fs, shader_extensions());
    } else {
      module->shader = new ShaderProgram(vs, fs, gs, shader_extensions());
    }
  } else {
    module->shader = new ShaderProgram(ShaderProgram::load_from_file(vs, fs, gs, shader_extensions()));
  }

  CHECK_GL();
}

void qb_shadermodule_destroy(qbShaderModule* shader) {
  delete *shader;
  *shader = nullptr;
}

void qb_gpubuffer_create(qbGpuBuffer* buffer_ref, qbGpuBufferAttr attr) {
  *buffer_ref = new qbGpuBuffer_;
  qbGpuBuffer buffer = *buffer_ref;
  buffer->size = attr->size;
  buffer->elem_size = attr->elem_size;
  buffer->buffer_type = attr->buffer_type;
  buffer->sharing_type = attr->sharing_type;
  buffer->name = STRDUP(attr->name);
  buffer->ext = attr->ext;

  // In Vulkan this might be able to be replaced with a staging buffer.
  buffer->data = new char[buffer->size];
  if (attr->data) {
    memcpy(buffer->data, attr->data, buffer->size);
  }

  // TODO: consider glMapBuffer, glMapBufferRange, glSubBufferData
  // https://stackoverflow.com/questions/32222574/is-it-better-glbuffersubdata-or-glmapbuffer
  // https://www.khronos.org/opengl/wiki/Buffer_Object#Copying
  glGenBuffers(1, &buffer->id);
  GLenum target = TranslateQbGpuBufferTypeToOpenGl(buffer->buffer_type);
  glBindBuffer(target, buffer->id);
  glBufferData(target, buffer->size, buffer->data, GL_DYNAMIC_DRAW);
  CHECK_GL();
}

void qb_gpubuffer_destroy(qbGpuBuffer* buffer) {
  free((void*)(*buffer)->name);
  qb_renderext_destroy(&(*buffer)->ext);
  glDeleteBuffers(1, &(*buffer)->id);
  delete[] (*buffer)->data;
  delete *buffer;
  *buffer = nullptr;
}

const char* qb_gpubuffer_name(qbGpuBuffer buffer) {
  return buffer->name;
}

qbRenderExt qb_gpubuffer_ext(qbGpuBuffer buffer) {
  return buffer->ext;
}

void qb_gpubuffer_update(qbGpuBuffer buffer, intptr_t offset, size_t size, void* data) {
  GLenum target = TranslateQbGpuBufferTypeToOpenGl(buffer->buffer_type);
  glBindBuffer(target, buffer->id);
  glBufferSubData(target, offset, size, data);
  CHECK_GL();
}

void qb_gpubuffer_copy(qbGpuBuffer dst, qbGpuBuffer src,
                       intptr_t dst_offset, intptr_t src_offset, size_t size) {
  glBindBuffer(GL_COPY_WRITE_BUFFER, dst->id);
  glBindBuffer(GL_COPY_READ_BUFFER, src->id);
  glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, src_offset, dst_offset, size);
  CHECK_GL();
}

void qb_gpubuffer_swap(qbGpuBuffer a, qbGpuBuffer b) {
  std::swap(*a, *b);
}

size_t qb_gpubuffer_size(qbGpuBuffer buffer) {
  return buffer->size;
}

void qb_gpubuffer_resize(qbGpuBuffer buffer, size_t new_size) {
  if (buffer->size == new_size) {
    return;
  }

  size_t copy_size = std::min(buffer->size, new_size);
  char* new_data = new char[new_size];
  memcpy(new_data, buffer->data, copy_size);

  delete[] buffer->data;
  buffer->data = new_data;
  buffer->size = new_size;

  GLenum target = TranslateQbGpuBufferTypeToOpenGl(buffer->buffer_type);
  glBindBuffer(target, buffer->id);
  glBufferData(target, buffer->size, buffer->data, GL_DYNAMIC_DRAW);
  CHECK_GL();
}

QB_API void* qb_gpubuffer_map(qbGpuBuffer buffer, qbBufferAccess access) {
  GLenum target = TranslateQbGpuBufferTypeToOpenGl(buffer->buffer_type);
  GLenum access = TranslateQbBufferAccessToOpenGl(access);
  glBindBuffer(target, buffer->id);
  return glMapBuffer(target, access);
}

QB_API void qb_gpubuffer_unmap(qbGpuBuffer buffer) {
  GLenum target = TranslateQbGpuBufferTypeToOpenGl(buffer->buffer_type);
  glBindBuffer(target, buffer->id);
  glUnmapBuffer(target);
}

void qb_meshbuffer_create(qbMeshBuffer* buffer_ref, qbMeshBufferAttr attr) {
  *buffer_ref = new qbMeshBuffer_;
  qbMeshBuffer buffer = *buffer_ref;
  qbGeometryDescriptor_* descriptor = &buffer->descriptor;

  descriptor->bindings_count = attr->descriptor.bindings_count;
  descriptor->bindings = new qbBufferBinding_[descriptor->bindings_count];
  memcpy(descriptor->bindings, attr->descriptor.bindings,
         descriptor->bindings_count * sizeof(qbBufferBinding_));

  descriptor->attributes_count = attr->descriptor.attributes_count;
  descriptor->attributes = new qbVertexAttribute_[descriptor->attributes_count];
  memcpy(descriptor->attributes, attr->descriptor.attributes,
         descriptor->attributes_count * sizeof(qbVertexAttribute_));

  glGenVertexArrays(1, &buffer->id);
  buffer->vertices = new qbGpuBuffer[descriptor->bindings_count];

  buffer->uniforms_count = 0;
  buffer->uniforms = nullptr;
  buffer->uniform_bindings = nullptr;

  buffer->sampler_bindings = {};
  buffer->images = {};
  buffer->instance_count = attr->instance_count;
  buffer->name = STRDUP(attr->name);
  buffer->ext = attr->ext;

  CHECK_GL();
}

void qb_meshbuffer_destroy(qbMeshBuffer* buffer_ref) {
  free((void*)(*buffer_ref)->name);
  qb_renderext_destroy(&(*buffer_ref)->ext);
  glDeleteVertexArrays(1, &(*buffer_ref)->id);

  delete[](*buffer_ref)->descriptor.bindings;
  delete[](*buffer_ref)->descriptor.attributes;
  delete[](*buffer_ref)->vertices;
  delete[](*buffer_ref)->uniforms;
  delete[](*buffer_ref)->uniform_bindings;
  delete *buffer_ref;
}

const char* qb_meshbuffer_name(qbMeshBuffer buffer) {
  return buffer->name;
}

qbRenderExt qb_meshbuffer_ext(qbMeshBuffer buffer) {
  return buffer->ext;
}

void qb_meshbuffer_attachimages(qbMeshBuffer buffer, size_t count, uint32_t bindings[], qbImage images[]) {
  if (!buffer->images.empty() != 0) {
    FATAL("qbMeshBuffer already has attached images. Use qb_meshbuffer_updateimages to change attached images");
  }

  buffer->sampler_bindings.reserve(count);
  buffer->images.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    buffer->sampler_bindings.push_back(bindings[i]);
    buffer->images.push_back(images[i]);
  }
}

void qb_meshbuffer_updateimages(qbMeshBuffer buffer, size_t count, uint32_t bindings[], qbImage images[]) {
  buffer->sampler_bindings.resize(count);
  buffer->images.resize(count);

  for (size_t i = 0; i < count; ++i) {
    buffer->sampler_bindings[i] = bindings[i];
    buffer->images[i] = images[i];
  }
}

void qb_meshbuffer_attachuniforms(qbMeshBuffer buffer, size_t count, uint32_t bindings[], qbGpuBuffer uniforms[]) {
  buffer->uniforms_count = count;
  buffer->uniforms = new qbGpuBuffer[buffer->uniforms_count];
  buffer->uniform_bindings = new uint32_t[buffer->uniforms_count];

  for (uint32_t i = 0; i < buffer->uniforms_count; ++i) {
    buffer->uniforms[i] = uniforms[i];
    buffer->uniform_bindings[i] = bindings[i];
  }
}

void qb_meshbuffer_attachvertices(qbMeshBuffer buffer, qbGpuBuffer vertices[], size_t count) {
  glBindVertexArray(buffer->id);

  for (size_t i = 0; i < buffer->descriptor.bindings_count; ++i) {
    qbBufferBinding binding = buffer->descriptor.bindings + i;
    buffer->vertices[i] = vertices[binding->binding];
    if (!vertices[binding->binding]) {
      FATAL("Vertices[ " << binding->binding << " ] is null");
    }

    qbGpuBuffer gpu_buffer = buffer->vertices[i];
    GLenum target = TranslateQbGpuBufferTypeToOpenGl(gpu_buffer->buffer_type);
    glBindBuffer(target, gpu_buffer->id);
    CHECK_GL();

    for (size_t j = 0; j < buffer->descriptor.attributes_count; ++j) {
      qbVertexAttribute vattr = buffer->descriptor.attributes + j;
      if (vattr->binding == binding->binding) {
        glEnableVertexAttribArray(vattr->location);
        glVertexAttribPointer(vattr->location,
          (GLint)vattr->count,
                              TranslateQbVertexAttribTypeToOpenGl(vattr->type),
                              vattr->normalized,
                              binding->stride,
                              vattr->offset);
        if (binding->input_rate) {
          glVertexAttribDivisor(vattr->location, 1);
        }
        CHECK_GL();
      }
    }
  }

  if (!buffer->indices) {
    buffer->count = count;
  }
}

void qb_meshbuffer_attachindices(qbMeshBuffer buffer, qbGpuBuffer indices, size_t count) {
  glBindVertexArray(buffer->id);
  glBindBuffer(TranslateQbGpuBufferTypeToOpenGl(indices->buffer_type), indices->id);
  glBufferData(TranslateQbGpuBufferTypeToOpenGl(indices->buffer_type), indices->size, indices->data, GL_DYNAMIC_DRAW);

  if (!indices) {
    FATAL("Indices are null");
  }

  buffer->indices = indices;
  buffer->count = count;
  CHECK_GL();
}

void qb_meshbuffer_render(qbMeshBuffer buffer, qbDrawMode mode, uint32_t bindings[]) {
  // Bind textures.
  for (size_t i = 0; i < buffer->images.size(); ++i) {
    glActiveTexture((GLenum)(GL_TEXTURE0 + i));

    qbImageType type;
    uint32_t image_id = 0;
    uint32_t module_binding = bindings[i];

    for (size_t j = 0; j < buffer->images.size(); ++j) {
      uint32_t buffer_binding = buffer->sampler_bindings[j];
      if (module_binding == buffer_binding) {
        type = buffer->images[j]->type;
        image_id = buffer->images[j]->id;
        break;
      }
    }

    glBindTexture(TranslateQbImageTypeToOpenGl(type), (GLuint)image_id);
  }

  // Bind uniform blocks for this draw buffer.
  for (size_t i = 0; i < buffer->uniforms_count; ++i) {
    qbGpuBuffer uniform = buffer->uniforms[i];
    uint32_t binding = buffer->uniform_bindings[i];
    glBindBufferBase(GL_UNIFORM_BUFFER, binding, uniform->id);
  }

  // Render elements
  glBindVertexArray((GLuint)buffer->id);
  qbGpuBuffer indices = buffer->indices;
  GLenum gl_mode = TranslateQbDrawModeToOpenGlMode(mode);
  if (indices) {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices->id);    
    if (buffer->instance_count == 0) {
      glDrawElements(gl_mode, (GLsizei)buffer->count, GL_UNSIGNED_INT, nullptr);
    } else {
      glDrawElementsInstanced(gl_mode, (GLsizei)buffer->count, GL_UNSIGNED_INT, nullptr, (GLsizei)buffer->instance_count);
    }
  } else {
    if (buffer->instance_count == 0) {
      glDrawArrays(gl_mode, 0, (GLsizei)buffer->count);
    } else {
      glDrawArraysInstanced(gl_mode, 0, (GLsizei)buffer->count, (GLsizei)buffer->instance_count);
    }
  }
  CHECK_GL();
}

size_t qb_meshbuffer_vertices(qbMeshBuffer buffer, qbGpuBuffer** vertices) {
  *vertices = buffer->vertices;
  return buffer->descriptor.bindings_count;
}

void qb_meshbuffer_indices(qbMeshBuffer buffer, qbGpuBuffer* indices) {
  *indices = buffer->indices;
}

size_t qb_meshbuffer_images(qbMeshBuffer buffer, qbImage** images) {
  *images = buffer->images.data();
  return buffer->images.size();
}

size_t qb_meshbuffer_uniforms(qbMeshBuffer buffer, qbGpuBuffer** uniforms) {
  *uniforms = buffer->uniforms;
  return buffer->uniforms_count;
}

size_t qb_meshbuffer_getcount(qbMeshBuffer buffer) {
  return buffer->count;
}

void qb_meshbuffer_setcount(qbMeshBuffer buffer, size_t count) {
  buffer->count = count;
}

void qb_renderpass_create(qbRenderPass* render_pass_ref, qbRenderPassAttr attr) {
  *render_pass_ref = new qbRenderPass_;
  qbRenderPass render_pass = *render_pass_ref;

  render_pass->attachments.reserve(attr->attachments_count);
  for (size_t i = 0; i < attr->attachments_count; ++i) {
    render_pass->attachments.push_back(attr->attachments[i]);
  }
}

void qb_renderpass_destroy(qbRenderPass* render_pass) {
  free((void*)(*render_pass)->name);
  qb_renderext_destroy(&(*render_pass)->ext);

  delete *render_pass;
  *render_pass = nullptr;
}

const char* qb_renderpass_name(qbRenderPass render_pass) {
  return render_pass->name;
}

qbRenderExt qb_renderpass_ext(qbRenderPass render_pass) {
  return render_pass->ext;
}

void qb_imagesampler_create(qbImageSampler* sampler_ref, qbImageSamplerAttr attr) {
  //https://orangepalantir.org/topicspace/show/84
  qbImageSampler sampler = *sampler_ref = new qbImageSampler_;
  sampler->attr = *attr;
  sampler->name = nullptr;
  glGenSamplers(1, &sampler->id);
  glSamplerParameteri(sampler->id, GL_TEXTURE_WRAP_S,
                      TranslateQbImageWrapTypeToOpenGl(attr->s_wrap));
  CHECK_GL();
  glSamplerParameteri(sampler->id, GL_TEXTURE_WRAP_T,
                      TranslateQbImageWrapTypeToOpenGl(attr->t_wrap));
  CHECK_GL();
  glSamplerParameteri(sampler->id, GL_TEXTURE_WRAP_R,
                      TranslateQbImageWrapTypeToOpenGl(attr->r_wrap));
  CHECK_GL();
  glSamplerParameteri(sampler->id, GL_TEXTURE_MAG_FILTER,
                      TranslateQbFilterTypeToOpenGl(attr->mag_filter));
  CHECK_GL();
  glSamplerParameteri(sampler->id, GL_TEXTURE_MIN_FILTER,
                      TranslateQbFilterTypeToOpenGl(attr->min_filter));
  CHECK_GL();
}

void qb_imagesampler_destroy(qbImageSampler* sampler_ref) {
  free((void*)(*sampler_ref)->name);
  glDeleteSamplers(1, &(*sampler_ref)->id);
  delete *sampler_ref;
  *sampler_ref = nullptr;
}

const char* qb_imagesampler_name(qbImageSampler sampler) {
  return sampler->name;
}

void qb_image_create(qbImage* image_ref, qbImageAttr attr, qbPixelMap pixel_map) {
  qbImage image = *image_ref = new qbImage_;
  image->type = attr->type;
  image->format = pixel_map->format;
  image->name = STRDUP(attr->name);
  image->ext = attr->ext;


  // Load the image from the file into SDL's surface representation
  GLenum image_type = TranslateQbImageTypeToOpenGl(image->type);
  
  glGenTextures(1, &image->id);
  glBindTexture(image_type, image->id);

  int stored_alignment;
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &stored_alignment);

  qbRenderExt found = qb_renderext_find(image->ext, "qbPixelAlignmentOglExt_");
  if (found) {
    qbPixelAlignmentOglExt_* u_ext = (qbPixelAlignmentOglExt_*)found->ext;
    glPixelStorei(GL_UNPACK_ALIGNMENT, u_ext->alignment);
  }

  GLenum format = TranslateQbPixelFormatToOpenGl(image->format);
  GLenum internal_format = TranslateQbPixelFormatToInternalOpenGl(image->format);
  GLenum type = TranslateQbPixelFormatToOpenGlSize(image->format);
  if (image_type == GL_TEXTURE_1D) {
    glTexImage1D(image_type, 0, internal_format,
                 pixel_map->width * pixel_map->height,
                 0, format, type, pixel_map->pixels);
  } else if (image_type == GL_TEXTURE_2D) {
    glTexImage2D(image_type, 0, internal_format, pixel_map->width, pixel_map->height,
                 0, format, type, pixel_map->pixels);
  } else if (image_type == GL_TEXTURE_3D) {
    glTexImage3D(image_type, 0, internal_format, pixel_map->width, pixel_map->height, pixel_map->depth,
                 0, format, type, pixel_map->pixels);
  }
  if (attr->generate_mipmaps) {
    glGenerateMipmap(image_type);
  }

  image->width = pixel_map->width;
  image->height = pixel_map->height;
  
  glPixelStorei(GL_UNPACK_ALIGNMENT, stored_alignment);
  CHECK_GL();
}

void qb_image_raw(qbImage* image_ref, qbImageAttr attr, qbPixelFormat format, uint32_t width, uint32_t height, void* pixels) {
  qbImage image = *image_ref = new qbImage_;
  image->type = attr->type;
  image->format = format;
  image->name = STRDUP(attr->name);
  image->ext = attr->ext;
  image->width = width;
  image->height = height;

  // Load the image from the file into SDL's surface representation
  GLenum image_type = TranslateQbImageTypeToOpenGl(image->type);

  glGenTextures(1, &image->id);
  glBindTexture(image_type, image->id);

  int stored_alignment;
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &stored_alignment);

  qbRenderExt found = qb_renderext_find(image->ext, "qbPixelAlignmentOglExt_");
  if (found) {
    qbPixelAlignmentOglExt_* u_ext = (qbPixelAlignmentOglExt_*)found->ext;
    glPixelStorei(GL_UNPACK_ALIGNMENT, u_ext->alignment);
  }

  GLenum pixel_format = TranslateQbPixelFormatToOpenGl(image->format);
  GLenum internal_format = TranslateQbPixelFormatToInternalOpenGl(image->format);
  GLenum type = TranslateQbPixelFormatToOpenGlSize(image->format);
  if (image_type == GL_TEXTURE_1D) {
    glTexImage1D(image_type, 0, internal_format,
                 width * height,
                 0, pixel_format, type, pixels);
  } else if (image_type == GL_TEXTURE_2D) {
    glTexImage2D(image_type, 0, internal_format, width, height,
                 0, pixel_format, type, pixels);
  }
  if (attr->generate_mipmaps) {
    glGenerateMipmap(image_type);
  }

  glPixelStorei(GL_UNPACK_ALIGNMENT, stored_alignment);
  CHECK_GL();
}

void qb_image_destroy(qbImage* image) {
  free((void*)(*image)->name);
  glDeleteTextures(1, &(*image)->id);
  qb_renderext_destroy(&(*image)->ext);
  delete *image;
  *image = nullptr;
}

void qb_image_copy(qbImage src_image, qbImage* dest_image) {

}

const char* qb_image_name(qbImage image) {
  return image->name;
}

void qb_image_load(qbImage* image_ref, qbImageAttr attr, const char* file) {
  qbImage image = *image_ref = new qbImage_{};
  image->type = attr->type;
  
  // Load the image from the file into SDL's surface representation
  int w, h, n;
  unsigned char* pixels = stbi_load(file, &w, &h, &n, 0);

  if (!pixels) {
    std::cout << "Could not load texture " << file << ": " << stbi_failure_reason() << std::endl;
    return;
  }

  image->width = w;
  image->height = h;

  GLenum image_type = TranslateQbImageTypeToOpenGl(attr->type);
  
  glGenTextures(1, &image->id);
  glBindTexture(image_type, image->id);

  if (image_type == GL_TEXTURE_1D) {
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA8, w * h, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
  } else if (image_type == GL_TEXTURE_2D) {
    if (n == 1) {
      image->format = qbPixelFormat::QB_PIXEL_FORMAT_R8;
      glTexImage2D(image_type, 0, GL_RED, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, pixels);
    } else if (n == 2) {
      image->format = qbPixelFormat::QB_PIXEL_FORMAT_RG8;
      glTexImage2D(image_type, 0, GL_RG, w, h, 0, GL_RG, GL_UNSIGNED_BYTE, pixels);
    } else if (n == 3) {
      image->format = qbPixelFormat::QB_PIXEL_FORMAT_RGB8;
      glTexImage2D(image_type, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    } else if (n == 4) {
      image->format = qbPixelFormat::QB_PIXEL_FORMAT_RGBA8;
      glTexImage2D(image_type, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    } else {
      assert(false && "Received an unsupported amount of channels");
    }
  }
  if (attr->generate_mipmaps) {
    glGenerateMipmap(image_type);
  }

  stbi_image_free(pixels);
  CHECK_GL();
}

void qb_image_update(qbImage image, ivec3s offset, ivec3s sizes, void* data) {
  GLenum image_type = TranslateQbImageTypeToOpenGl(image->type);
  GLenum format = TranslateQbPixelFormatToOpenGl(image->format);
  GLenum type = TranslateQbPixelFormatToOpenGlSize(image->format);

  glBindTexture(image_type, image->id);

  int stored_alignment;
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &stored_alignment);
  qbRenderExt found = qb_renderext_find(image->ext, "qbPixelAlignmentOglExt_");
  if (found) {
    qbPixelAlignmentOglExt_* u_ext = (qbPixelAlignmentOglExt_*)found->ext;
    glPixelStorei(GL_UNPACK_ALIGNMENT, u_ext->alignment);
  }

  if (image_type == GL_TEXTURE_1D) {
    glTexSubImage1D(image_type, 0, offset.x, sizes.x, format, type, data);
  } else if (image_type == GL_TEXTURE_2D) {
    glTexSubImage2D(image_type, 0, offset.x, offset.y, sizes.x, sizes.y, format, type, data);
  } else if (image_type == GL_TEXTURE_3D) {
    glTexSubImage3D(image_type, 0, offset.x, offset.y, offset.z, sizes.x, sizes.y, sizes.z, format, type, data);
  }

  glPixelStorei(GL_UNPACK_ALIGNMENT, stored_alignment);
  CHECK_GL();
}

int qb_image_width(qbImage image) {
  return image->width;
}

int qb_image_height(qbImage image) {
  return image->height;
}

qbImage qb_framebuffer_createdrawbuffer(uint32_t color_binding, uint32_t width, uint32_t height) {
  qbImage ret = nullptr;
  qbImageAttr_ image_attr = {};
  image_attr.type = QB_IMAGE_TYPE_2D;
  qb_image_raw(&ret, &image_attr,
                qbPixelFormat::QB_PIXEL_FORMAT_RGBA8, width, height, nullptr);
  return ret;
}

void qb_framebuffer_bindbuffer(qbFramebufferAttachment attachment, uint32_t color_binding) {
  if (!attachment->image) {
    return;
  }

  qbImage image = attachment->image;
  switch (attachment->aspect) {
    case QB_COLOR_ASPECT:
      glBindTexture(GL_TEXTURE_2D, image->id);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + color_binding, GL_TEXTURE_2D, image->id, 0);
      CHECK_GL();
      return;
    case QB_DEPTH_ASPECT:
      glBindTexture(GL_TEXTURE_2D, image->id);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, image->id, 0);
      CHECK_GL();
      return;
    case QB_STENCIL_ASPECT:
      glBindTexture(GL_TEXTURE_2D, image->id);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, image->id, 0);
      CHECK_GL();
      return;
    case QB_DEPTH_STENCIL_ASPECT:
      glBindTexture(GL_TEXTURE_2D, image->id);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, image->id, 0);
      CHECK_GL();
      return;
  }
}

void qb_framebuffer_init(qbFrameBuffer frame_buffer, qbFrameBufferAttr attr) {
  uint32_t frame_buffer_id = 0;
  glGenFramebuffers(1, &frame_buffer_id);
  frame_buffer->id = frame_buffer_id;

  frame_buffer->attachments.reserve(attr->attachments_count);
  for (size_t i = 0; i < attr->attachments_count; ++i) {
    frame_buffer->attachments.push_back(attr->attachments[i]);
  }

  CHECK_GL();
  glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer_id);
  CHECK_GL();

  uint32_t color_binding = 0;

  std::vector<qbImage> draw_buffers;
  for (size_t i = 0; i < attr->attachments_count; ++i) {
    qbFramebufferAttachment attachment = &attr->attachments[i];
    
    qb_framebuffer_bindbuffer(attachment, color_binding);
    if (attachment->aspect == QB_COLOR_ASPECT) {
      draw_buffers.push_back(attachment->image);
      ++color_binding;
    }
  }

  if (!draw_buffers.empty()) {
    GLenum draw_buffers_bindings[16] = {};
    for (size_t i = 0; i < draw_buffers.size(); ++i) {
      draw_buffers_bindings[i] = (GLenum)(GL_COLOR_ATTACHMENT0 + i);
      frame_buffer->draw_buffers.push_back((GLenum)(GL_COLOR_ATTACHMENT0 + i));
    }
    glDrawBuffers((GLsizei)attr->attachments_count, draw_buffers_bindings);
    CHECK_GL();
  } else {
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
  }

  frame_buffer->render_targets = std::move(draw_buffers);

  GLenum result = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  CHECK_GL();
  if (result != GL_FRAMEBUFFER_COMPLETE) {
    FATAL("Error creating FBO: " << result);
  }
  CHECK_GL();
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void qb_framebuffer_clear(qbFrameBuffer frame_buffer) {
  for (qbImage& img : frame_buffer->render_targets) {
    qb_image_destroy(&img);
  }
  qb_image_destroy(&frame_buffer->depth_target);
  qb_image_destroy(&frame_buffer->stencil_target);
  qb_image_destroy(&frame_buffer->depthstencil_target);
  
  frame_buffer->render_targets = {};
  frame_buffer->depth_target = nullptr;
  frame_buffer->stencil_target = nullptr;
  frame_buffer->depthstencil_target = nullptr;
}

void qb_framebuffer_create(qbFrameBuffer* frame_buffer, qbFrameBufferAttr attr) {
  *frame_buffer = new qbFrameBuffer_{};
  qb_framebuffer_init(*frame_buffer, attr);
}

void qb_framebuffer_destroy(qbFrameBuffer* frame_buffer) {
  delete *frame_buffer;
  *frame_buffer = nullptr;
}

qbImage qb_framebuffer_gettarget(qbFrameBuffer frame_buffer, size_t i) {
  assert(i < frame_buffer->render_targets.size());
  return frame_buffer->render_targets[i];
}

void qb_framebuffer_settarget(qbFrameBuffer frame_buffer, size_t i, qbImage image) {
  assert(i < frame_buffer->render_targets.size());

  glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer->id);
  glBindTexture(GL_TEXTURE_2D, image->id);
  glFramebufferTexture2D(GL_FRAMEBUFFER, (GLenum)(GL_COLOR_ATTACHMENT0 + i), GL_TEXTURE_2D, image->id, 0);
  CHECK_GL();
}

size_t qb_framebuffer_rendertargets(qbFrameBuffer frame_buffer, qbImage** targets) {
  *targets = frame_buffer->render_targets.data();
  return frame_buffer->render_targets.size();
}

qbImage qb_framebuffer_depthtarget(qbFrameBuffer frame_buffer) {
  return frame_buffer->depth_target;
}

qbImage qb_framebuffer_stenciltarget(qbFrameBuffer frame_buffer) {
  return frame_buffer->stencil_target;
}

qbImage qb_framebuffer_depthstenciltarget(qbFrameBuffer frame_buffer) {
  return frame_buffer->depthstencil_target;
}

void qb_framebuffer_resize(qbFrameBuffer frame_buffer, uint32_t width, uint32_t height) {
  for (qbImage& img : frame_buffer->render_targets) {
    qb_image_destroy(&img);
  }
  if (frame_buffer->depth_target) {
    qb_image_destroy(&frame_buffer->depth_target);
  }

  if (frame_buffer->stencil_target) {
    qb_image_destroy(&frame_buffer->stencil_target);
  }

  if (frame_buffer->depthstencil_target) {
    qb_image_destroy(&frame_buffer->depthstencil_target);
  }

  glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer->id);
  glDeleteFramebuffers(1, &frame_buffer->id);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  frame_buffer->render_targets = {};
  frame_buffer->depth_target = nullptr;
  frame_buffer->stencil_target = nullptr;
  frame_buffer->depthstencil_target = nullptr;

  //frame_buffer->attr.width = width;
  //frame_buffer->attr.height = height;
  //qb_framebuffer_init(frame_buffer, &frame_buffer->attr);
}

uint32_t qb_framebuffer_width(qbFrameBuffer frame_buffer) {
  return 0;//frame_buffer->attr.width;
}

uint32_t qb_framebuffer_height(qbFrameBuffer frame_buffer) {
  return 0;//frame_buffer->attr.height;
}

uint32_t qb_framebuffer_readpixel(qbFrameBuffer frame_buffer, uint32_t attachment_binding, int32_t x, int32_t y) {
  uint32_t id = frame_buffer ? frame_buffer->id : 0;

  glBindFramebuffer(GL_READ_BUFFER, id);
  glReadBuffer(GL_COLOR_ATTACHMENT0 + attachment_binding);

  uint32_t pixel;
  glReadPixels(x, y, 1, 1,
               TranslateQbPixelFormatToOpenGl(qbPixelFormat::QB_PIXEL_FORMAT_RGBA8),
               TranslateQbPixelFormatToOpenGlSize(qbPixelFormat::QB_PIXEL_FORMAT_RGBA8), &pixel);

  glBindFramebuffer(GL_READ_BUFFER, 0);
  return pixel;
}

#if OLD_API
void qb_framebuffer_blit(qbFrameBuffer src, qbFrameBuffer dest,
                         qbViewport_ src_viewport,
                         qbViewport_ dest_viewport,
                         qbFrameBufferAttachment mask, qbFilterType filter) {
  glBindFramebuffer(GL_READ_FRAMEBUFFER, src->id);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dest->id);
  GLenum gl_mask = TranslateQbFrameBufferAttachmentToOpenGl(mask);
  GLenum gl_filter = TranslateQbFilterTypeToOpenGl(filter);
  glBlitFramebuffer((GLint)src_viewport.x,
                    (GLint)src_viewport.y, 
                    (GLint)(src_viewport.x + src_viewport.w),
                    (GLint)(src_viewport.y + src_viewport.h),
                    (GLint)dest_viewport.x,
                    (GLint)dest_viewport.y,
                    (GLint)(dest_viewport.x + dest_viewport.w),
                    (GLint)(dest_viewport.y + dest_viewport.h),
                    gl_mask, gl_filter);
  CHECK_GL();
  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}
#endif

qbRenderExt qb_renderext_find(qbRenderExt extensions, const char* ext_name) {
  while (extensions && extensions->name && strcmp(extensions->name, ext_name) != 0) {
    extensions = extensions->next;
  }
  return extensions;
}

void qb_renderext_destroy(qbRenderExt* extensions) {
  if (!extensions) {
    return;
  }

  qbRenderExt next = *extensions;
  while (next) {
    qbRenderExt to_delete = next;
    next = (*extensions)->next;
    delete to_delete;
  }
}

void qb_renderext_add(qbRenderExt* extensions, const char* ext_name, void* data) {
  qbRenderExt new_ext = new qbRenderExt_;
  new_ext->next = *extensions;
  new_ext->name = ext_name;
  new_ext->ext = data;

  *extensions = new_ext;
}

void qb_swapchain_create(qbSwapchain* swapchain, qbSwapchainAttr attr) {
  *swapchain = new qbSwapchain_{};
  (*swapchain)->images.resize(2);
  (*swapchain)->tasks.resize(2);
  (*swapchain)->extent = attr->extent;

  qbImageAttr_ image_attr{
      .type = qbImageType::QB_IMAGE_TYPE_2D
  };
  qb_image_raw(&(*swapchain)->images[0], &image_attr, qbPixelFormat::QB_PIXEL_FORMAT_RGB8,
               (uint32_t)attr->extent.w, (uint32_t)attr->extent.h, nullptr);
  qb_image_raw(&(*swapchain)->images[1], &image_attr, qbPixelFormat::QB_PIXEL_FORMAT_RGB8,
               (uint32_t)attr->extent.w, (uint32_t)attr->extent.h, nullptr);
  (*swapchain)->front_image = 0;
  (*swapchain)->back_image = 1;
  (*swapchain)->front_task = qbInvalidHandle;
  (*swapchain)->back_task = qbInvalidHandle;

  glGenVertexArrays(1, &(*swapchain)->present_vao);
  glGenBuffers(1, &(*swapchain)->present_vbo);

  glBindVertexArray((*swapchain)->present_vao);
  glBindBuffer(GL_ARRAY_BUFFER, (*swapchain)->present_vbo);

  float vertices[] = {
    // Positions   // UVs
    -1.0, -1.0,    0.0, 0.0,
     1.0, -1.0,    1.0, 0.0,
     1.0,  1.0,    1.0, 1.0,
    -1.0,  1.0,    0.0, 1.0
  };
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

  {
    qbShaderModuleAttr_ attr = {};
    attr.vs = get_presentpass_vs();
    attr.fs = get_presentpass_fs();
    attr.interpret_as_strings = true;

    qb_shadermodule_create(&(*swapchain)->shader_module, &attr);
  }
  {
    qbImageSamplerAttr_ attr = {};
    attr.image_type = QB_IMAGE_TYPE_2D;
    attr.min_filter = QB_FILTER_TYPE_LINEAR;
    attr.mag_filter = QB_FILTER_TYPE_LINEAR;
    qb_imagesampler_create(&(*swapchain)->swapchain_imager_sampler, &attr);
  }
  CHECK_GL();
}

void qb_swapchain_images(qbSwapchain swapchain, size_t* count, qbImage* images) {
  if (count) {
    *count = swapchain->images.size();
  }

  if (images) {
    for (size_t i = 0; i < swapchain->images.size(); ++i) {
      images[i] = swapchain->images[i];
    }    
  }
}

void qb_swapchain_swap(qbSwapchain swapchain) {
  qb_task_join(swapchain->front_task);

  std::unique_lock<std::mutex> l(swapchain->image_swap_mu);
  std::swap(swapchain->back_image, swapchain->front_image);
  std::swap(swapchain->back_task, swapchain->front_task);
}

uint32_t qb_swapchain_waitforframe(qbSwapchain swapchain) {
  qb_task_join(swapchain->back_task);

  std::unique_lock<std::mutex> l(swapchain->image_swap_mu);
  return swapchain->back_image;
}

void qb_swapchain_present(qbSwapchain swapchain, qbDrawPresentInfo present_info) {
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glClearColor(1.f, 0.f, 0.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);

  glDisable(GL_DEPTH_TEST);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_BLEND);

  glFrontFace(GL_CCW);

  CHECK_GL();
  qbShaderModule module = swapchain->shader_module;
  glViewport(0, 0, (GLsizei)swapchain->extent.w, (GLsizei)swapchain->extent.h);

  glBindVertexArray(swapchain->present_vao);
  module->shader->use();
  uint32_t program = (uint32_t)module->shader->id();

  glActiveTexture(GL_TEXTURE0);
  glUniform1i(glGetUniformLocation(program, "tex_sampler"), 0);
  glBindSampler(0, (GLuint)swapchain->swapchain_imager_sampler->id);
  CHECK_GL();

  qbImage present_image = swapchain->images[present_info->image_index];
  glBindTexture(
    TranslateQbImageTypeToOpenGl(present_image->type),
    (GLuint)present_image->id);
  CHECK_GL();

  // Render elements  
  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
  CHECK_GL();
}

void qb_shaderresourcelayout_create(qbShaderResourceLayout* resource_set, qbShaderResourceLayoutAttr attr) {
  (*resource_set) = new qbShaderResourceLayout_();
  for (uint32_t i = 0; i < attr->binding_count; ++i) {
    (*resource_set)->bindings.push_back(attr->bindings[i]);
  }
}

void qb_shaderresourceset_writeuniform(qbShaderResourceSet resource_set, uint32_t binding, qbGpuBuffer buffer) {
  for (size_t i = 0; i < resource_set->bindings.size(); ++i) {
    qbShaderResourceBinding resource_binding = resource_set->bindings[i];
    if (resource_binding->binding == binding && resource_binding->resource_type == QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER) {
      resource_set->uniforms[i] = buffer;
      break;
    }
  }
}

void qb_shaderresourceset_writeimage(qbShaderResourceSet resource_set, uint32_t binding, qbImage image, qbImageSampler sampler) {
  for (auto i = 0; i < resource_set->bindings.size(); ++i) {
    qbShaderResourceBinding resource_binding = resource_set->bindings[i];
    if (resource_set->bindings[i]->binding == binding && resource_binding->resource_type == QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER) {
      resource_set->images[i] = image;
      resource_set->samplers[i] = sampler;
      break;
    }
  }
}

void qb_shaderresourcepipelinelayout_create(qbShaderResourcePipelineLayout* layout, qbShaderResourcePipelineLayoutAttr attr) {
  (*layout) = new qbShaderResourcePipelineLayout_();
  for (uint32_t i = 0; i < attr->layout_count; ++i) {
    (*layout)->layouts.push_back(attr->layouts[i]);
  }
}

void qb_shaderresourceset_create(qbShaderResourceSet* resource_set, qbShaderResourceSetAttr attr) {
  (*resource_set) = new qbShaderResourceSet_();
  qbShaderResourceLayout layout = attr->layout;
  for (uint32_t i = 0; i < attr->create_count; ++i) {
    resource_set[i] = new qbShaderResourceSet_();
    for (auto& binding : layout->bindings) {
      resource_set[i]->bindings.push_back(&binding);
      resource_set[i]->uniforms.push_back({});
      resource_set[i]->images.push_back({});
      resource_set[i]->samplers.push_back({});
    }
  }
}

void qb_shaderresourceset_updateuniform(qbShaderResourceSet resource_set, uint32_t binding, qbGpuBuffer buffer) {

}

void qb_shaderresourceset_updatesampler(qbShaderResourceSet resource_set, uint32_t binding, qbGpuBuffer buffer) {

}

void qb_drawcmd_create(qbDrawCommandBuffer* cmd_buf, qbDrawCommandBufferAttr attr) {
  for (uint32_t i = 0; i < attr->count; ++i) {
    cmd_buf[i] = new qbDrawCommandBuffer_{attr->allocator};
  }
}

void qb_drawcmd_destroy(qbDrawCommandBuffer* cmd_buf, size_t count) {
  for (uint32_t i = 0; i < count; ++i) {
    qb_drawcmd_clear(cmd_buf[i]);
    delete cmd_buf[i];
  }
}

void qb_drawcmd_clear(qbDrawCommandBuffer cmd_buf) {
  cmd_buf->clear();
}

void qb_drawcmd_setcull(qbDrawCommandBuffer cmd_buf, qbFace cull_face) {
  qbRenderCommand_ c{ .type = QB_RENDER_COMMAND_SETCULL };
  c.command.set_cull = qbRenderCommandSetCull_{ cull_face };  

  cmd_buf->queue_command(&c);
}

void qb_drawcmd_beginpass(qbDrawCommandBuffer cmd_buf, qbBeginRenderPassInfo begin_info) {
  assert(begin_info->render_pass->attachments.size() >= begin_info->clear_values_count);

  cmd_buf->begin_pass(begin_info);
  qbRenderCommand_ c{ .type = QB_RENDER_COMMAND_BEGIN };
  c.command.begin = qbRenderCommandBegin_{};

  cmd_buf->queue_command(&c);
}

void qb_drawcmd_endpass(qbDrawCommandBuffer cmd_buf) {
  qbRenderCommand_ c{ .type = QB_RENDER_COMMAND_END };
  c.command.end = qbRenderCommandEnd_{};

  cmd_buf->queue_command(&c);
  cmd_buf->end_pass();
}

void qb_drawcmd_setviewport(qbDrawCommandBuffer cmd_buf, qbViewport viewport) {
  qbRenderCommand_ c{ .type = QB_RENDER_COMMAND_SETVIEWPORT };
  c.command.set_viewport = qbRenderCommandSetViewport_{.viewport = *viewport};

  cmd_buf->queue_command(&c);
}

void qb_drawcmd_setscissor(qbDrawCommandBuffer cmd_buf, qbRect rect) {
  qbRenderCommand_ c{ .type = QB_RENDER_COMMAND_SETSCISSOR };
  c.command.set_scissor = qbRenderCommandSetScissor_{ .rect = *rect };

  cmd_buf->queue_command(&c);
}

void qb_drawcmd_bindpipeline(qbDrawCommandBuffer cmd_buf, qbRenderPipeline pipeline, qbBool set_render_state) {
  qbRenderCommand_ c{ .type = QB_RENDER_COMMAND_BINDPIPELINE};
  c.command.bind_pipeline = qbRenderCommandBindPipeline_{ .pipeline = pipeline, .set_render_state = set_render_state };

  cmd_buf->queue_command(&c);
}

void qb_drawcmd_bindshaderresourceset(
  qbDrawCommandBuffer cmd_buf, qbShaderResourcePipelineLayout layout, qbShaderResourceSet resource_set) {

  qbRenderCommand_ c{ .type = QB_RENDER_COMMAND_BINDSHADERRESOURCESET };
  c.command.bind_shaderresourceset =
    qbRenderCommandBindShaderResourceSet_{ .layout = layout, .resource_set = resource_set };

  cmd_buf->queue_command(&c);
}

void qb_drawcmd_bindshaderresourcesets(
  qbDrawCommandBuffer cmd_buf, qbShaderResourcePipelineLayout layout,
  uint32_t resource_set_count, qbShaderResourceSet* resource_sets) {

  qbShaderResourceSet* resource_sets_copy = cmd_buf->allocate<qbShaderResourceSet>(resource_set_count);
  for (size_t i = 0; i < resource_set_count; ++i) {
    resource_sets_copy[i] = resource_sets[i];
  }

  qbRenderCommand_ c{ .type = QB_RENDER_COMMAND_BINDSHADERRESOURCESETS };
  c.command.bind_shaderresourcesets =
    qbRenderCommandBindShaderResourceSets_{ .layout = layout,
        .resource_set_count = resource_set_count,
        .resource_sets = resource_sets_copy };

  cmd_buf->queue_command(&c);
}

void qb_drawcmd_bindvertexbuffers(qbDrawCommandBuffer cmd_buf, uint32_t first_binding, uint32_t binding_count, qbGpuBuffer* buffers) {
  qbGpuBuffer* buffers_copy = cmd_buf->allocate<qbGpuBuffer>(binding_count);
  for (size_t i = 0; i < binding_count; ++i) {
    buffers_copy[i] = buffers[i];
  }

  qbRenderCommand_ c{ .type = QB_RENDER_COMMAND_BINDVERTEXBUFFERS };
  c.command.bind_vertexbuffers =
    qbRenderCommandBindVertexBuffers_{ .first_binding = first_binding,
        .binding_count = binding_count,
        .buffers = buffers_copy };

  cmd_buf->queue_command(&c);
}

void qb_drawcmd_bindindexbuffer(qbDrawCommandBuffer cmd_buf, qbGpuBuffer buffer) {
  qbRenderCommand_ c{ .type = QB_RENDER_COMMAND_BINDINDEXBUFFER };
  c.command.bind_indexbuffer = qbRenderCommandBindIndexBuffer_{ .buffer = buffer};

  cmd_buf->queue_command(&c);
}

void qb_drawcmd_draw(qbDrawCommandBuffer cmd_buf, uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) {
  qbRenderCommand_ c{ .type = QB_RENDER_COMMAND_DRAW };
  c.command.draw = qbRenderCommandDraw_{ .vertex_count = vertex_count, .instance_count = instance_count, .first_vertex = first_vertex, .first_instance = first_instance };

  cmd_buf->queue_command(&c);
}

void qb_drawcmd_drawindexed(qbDrawCommandBuffer cmd_buf, uint32_t index_count, uint32_t instance_count, uint32_t vertex_offset, uint32_t first_instance) {
  qbRenderCommand_ c{ .type = QB_RENDER_COMMAND_DRAWINDEXED };
  c.command.draw_indexed = qbRenderCommandDrawIndexed_{ .index_count = index_count, .instance_count = instance_count, .vertex_offset = vertex_offset, .first_instance = first_instance };

  cmd_buf->queue_command(&c);
}

void qb_drawcmd_updatebuffer(qbDrawCommandBuffer cmd_buf, qbGpuBuffer buffer, intptr_t offset, size_t size, void* data) {
  qbRenderCommand_ c{ .type = QB_RENDER_COMMAND_UPDATEBUFFER};
  c.command.update_buffer = qbRenderCommandUpdateBuffer_{ .buffer = buffer, .offset = offset, .size = size, .data = data };

  cmd_buf->queue_command(&c);
}

void qb_drawcmd_pushbuffer(qbDrawCommandBuffer cmd_buf, qbGpuBuffer buffer, intptr_t offset, size_t size, void* data) {
  void* data_copy = cmd_buf->allocate<void>(size);
  memcpy(data_copy, data, size);

  qbRenderCommand_ c{ .type = QB_RENDER_COMMAND_UPDATEBUFFER };
  c.command.update_buffer = qbRenderCommandUpdateBuffer_{ .buffer = buffer, .offset = offset, .size = size, .data = data_copy };

  cmd_buf->queue_command(&c);
}

void qb_drawcmd_subcommands(qbDrawCommandBuffer cmd_buf, qbDrawCommandBuffer to_draw) {
  qbRenderCommand_ c{ .type = QB_RENDER_COMMAND_SUBCOMMANDS };
  c.command.sub_commands = qbRenderCommandSubCommands_{ .cmd_buf = to_draw };

  cmd_buf->queue_command(&c);
}

qbTask qb_drawcmd_submit(qbDrawCommandBuffer cmd_buf, qbDrawCommandSubmitInfo submit_info) {
  return cmd_buf->submit(submit_info);
}

qbTask qb_drawcmd_flush(qbDrawCommandBuffer cmd_buf) {
  return cmd_buf->flush();
}
