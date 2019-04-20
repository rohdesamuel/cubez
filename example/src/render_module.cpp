#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include "render_module.h"
#include <cubez/common.h>
#include <algorithm>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include "render.h"
#include "shader.h"
#include <cubez/utils.h>

#ifdef _DEBUG
#define CHECK_GL()  {if (GLenum err = glGetError()) FATAL(gluErrorString(err)) }
#else
#define CHECK_GL()
#endif   

#define WITH_MODULE(module, fn, ...) (module)->render_interface.fn((module)->impl, __VA_ARGS__)

typedef struct qbRenderPipeline_ {
  std::vector<qbRenderPass> render_passes;
} qbRenderPipeline_;

typedef struct qbFrameBuffer_ {
  uint32_t id;
  qbImage render_target;
  qbImage depth_target;
  qbImage stencil_target;
  qbImage depthstencil_target;

} qbFrameBuffer_;

typedef struct qbImageSampler_ {
  qbImageSamplerAttr_ attr;
  uint32_t id;
  const char* name;
} qbImageSampler_;

typedef struct qbPixelMap_ {
  uint32_t width;
  uint32_t height;
  qbPixelFormat format;
  void* pixels;

  size_t size;
  size_t row_size;
  size_t pixel_size;

} qbPixelMap_;

typedef struct qbImage_ {
  const char* name;
  qbRenderExt ext;

  uint32_t id;
  qbImageType type;
  qbPixelFormat format;
} qbImage_;


typedef struct qbGpuBuffer_ {
  const char* name;
  qbRenderExt ext;

  void* data;
  size_t size;
  size_t elem_size;

  uint32_t id;
  qbGpuBufferType buffer_type;
  int sharing_type;
} qbGpuBuffer_;

typedef struct qbMeshBuffer_ {
  const char* name;
  qbRenderExt ext;

  uint32_t id;
  qbGpuBuffer indices;

  // Has the same count as bindings_count.
  qbGpuBuffer* vertices;

  uint32_t* uniform_bindings;
  qbGpuBuffer* uniforms;
  size_t uniforms_count;

  uint32_t* sampler_bindings;
  qbImage* images;
  size_t images_count;

  qbBufferBinding bindings;
  size_t bindings_count;

  qbVertexAttribute attributes;
  size_t attributes_count;

  size_t instance_count;
} qbMeshBuffer_;

typedef struct qbShaderModule_ {
  qbShader shader;

  qbShaderResourceInfo resources;
  size_t resources_count;

  uint32_t* uniform_bindings;
  qbGpuBuffer* uniforms;
  size_t uniforms_count;

  uint32_t* sampler_bindings;
  qbImageSampler* samplers;
  size_t samplers_count;

} qbShaderModule_;

typedef struct qbRenderPass_ {
  const char* name;
  qbRenderExt ext;

  uint32_t id;
  qbFrameBuffer frame_buffer;

  std::vector<qbMeshBuffer> draw_buffers;

  qbVertexAttribute attributes;
  size_t attributes_count;

  qbBufferBinding bindings;
  size_t bindings_count;

  qbShaderModule shader_program;

  glm::vec4 viewport;
  float viewport_scale;

  qbClearValue_ clear;

} qbRenderPass_;

namespace
{

GLenum TranslateQbVertexAttribTypeToOpenGl(qbVertexAttribType type) {
  switch (type) {
    case QB_VERTEX_ATTRIB_TYPE_BYTE: return GL_BYTE;
    case QB_VERTEX_ATTRIB_TYPE_UNSIGNED_BYTE: return GL_UNSIGNED_BYTE;
    case QB_VERTEX_ATTRIB_TYPE_SHORT: return GL_SHORT;
    case QB_VERTEX_ATTRIB_TYPE_UNSIGNED_SHORT: return GL_UNSIGNED_SHORT;
    case QB_VERTEX_ATTRIB_TYPE_INT: return GL_INT;
    case QB_VERTEX_ATTRIB_TYPE_UNSIGNED_INT: return GL_UNSIGNED_INT;
    case QB_VERTEX_ATTRIB_TYPE_HALF_FLOAT: return GL_HALF_FLOAT;
    case QB_VERTEX_ATTRIB_TYPE_FLOAT: return GL_FLOAT;
    case QB_VERTEX_ATTRIB_TYPE_DOUBLE: return GL_DOUBLE;
    case QB_VERTEX_ATTRIB_TYPE_FIXED: return GL_FIXED;
  }
  return 0;
}

GLenum TranslateQbGpuBufferTypeToOpenGl(qbGpuBufferType type) {
  switch (type) {
    case QB_GPU_BUFFER_TYPE_VERTEX: return GL_ARRAY_BUFFER;
    case QB_GPU_BUFFER_TYPE_INDEX: return GL_ELEMENT_ARRAY_BUFFER;
    case QB_GPU_BUFFER_TYPE_UNIFORM: return GL_UNIFORM_BUFFER;
  }
  return 0;
}

GLenum TranslateQbFilterTypeToOpenGl(qbFilterType type) {
  switch (type) {
    case QB_FILTER_TYPE_NEAREST: return GL_NEAREST;
    case QB_FILTER_TYPE_LINEAR: return GL_LINEAR;
    case QB_FILTER_TYPE_NEAREST_MIPMAP_NEAREST: return GL_NEAREST_MIPMAP_NEAREST;
    case QB_FILTER_TYPE_LINEAR_MIPMAP_NEAREST: return GL_LINEAR_MIPMAP_NEAREST;
    case QB_FILTER_TYPE_NEAREST_MIPMAP_LINEAR: return GL_NEAREST_MIPMAP_LINEAR;
    case QB_FILTER_TYPE_LIENAR_MIPMAP_LINEAR: return GL_LINEAR_MIPMAP_LINEAR;
  }
  return 0;
}
GLenum TranslateQbImageWrapTypeToOpenGl(qbImageWrapType type) {
  switch (type) {
    case QB_IMAGE_WRAP_TYPE_REPEAT: return GL_REPEAT;
    case QB_IMAGE_WRAP_TYPE_MIRRORED_REPEAT: return GL_MIRRORED_REPEAT;
    case QB_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE: return GL_CLAMP_TO_EDGE;
    case QB_IMAGE_WRAP_TYPE_CLAMP_TO_BORDER: return GL_CLAMP_TO_BORDER;
  }
  return 0;
}

GLenum TranslateQbImageTypeToOpenGl(qbImageType type) {
  switch (type) {
    case QB_IMAGE_TYPE_1D: return GL_TEXTURE_1D;
    case QB_IMAGE_TYPE_2D: return GL_TEXTURE_2D;
    case QB_IMAGE_TYPE_3D: return GL_TEXTURE_3D;
    case QB_IMAGE_TYPE_CUBE: return GL_TEXTURE_CUBE_MAP;
    case QB_IMAGE_TYPE_1D_ARRAY: return GL_TEXTURE_1D_ARRAY;
    case QB_IMAGE_TYPE_2D_ARRAY: return GL_TEXTURE_2D_ARRAY;
    case QB_IMAGE_TYPE_CUBE_ARRAY: return GL_TEXTURE_CUBE_MAP_ARRAY;
  }
  return 0;
}

GLenum TranslateQbPixelFormatToInternalOpenGl(qbPixelFormat format) {
  switch (format) {
    case QB_PIXEL_FORMAT_R8: return GL_R8;
    case QB_PIXEL_FORMAT_RG8: return GL_RGBA8;
    case QB_PIXEL_FORMAT_RGB8: return GL_RGBA8;
    case QB_PIXEL_FORMAT_RGBA8: return GL_RGBA8;
    case QB_PIXEL_FORMAT_D32: return GL_DEPTH_COMPONENT24;
    case QB_PIXEL_FORMAT_D24_S8: return GL_DEPTH24_STENCIL8;
    case QB_PIXEL_FORMAT_S8: return GL_STENCIL_INDEX;
  }
  return 0;
}

GLenum TranslateQbPixelFormatToOpenGl(qbPixelFormat format) {
  switch (format) {
    case QB_PIXEL_FORMAT_R8: return GL_RED;
    case QB_PIXEL_FORMAT_RG8: return GL_RG;
    case QB_PIXEL_FORMAT_RGB8: return GL_RGB;
    case QB_PIXEL_FORMAT_RGBA8: return GL_RGBA;
    case QB_PIXEL_FORMAT_D32: return GL_DEPTH_COMPONENT;
    case QB_PIXEL_FORMAT_D24_S8: return GL_DEPTH_STENCIL;
    case QB_PIXEL_FORMAT_S8: return GL_STENCIL_INDEX;
  }
  return 0;
}

GLenum TranslateQbPixelFormatToOpenGlSize(qbPixelFormat format) {
  switch (format) {
    case QB_PIXEL_FORMAT_R8:
    case QB_PIXEL_FORMAT_RG8:
    case QB_PIXEL_FORMAT_RGB8:
    case QB_PIXEL_FORMAT_RGBA8: return GL_UNSIGNED_BYTE;
    case QB_PIXEL_FORMAT_D32: return GL_FLOAT;
    case QB_PIXEL_FORMAT_D24_S8: return GL_UNSIGNED_INT_24_8;
    case QB_PIXEL_FORMAT_S8: return GL_UNSIGNED_BYTE;
  }
  return 0;
}

}

void qb_renderpass_draw(qbRenderPass render_pass);

qbPixelMap qb_pixelmap_create(uint32_t width, uint32_t height, qbPixelFormat format, void* pixels) {
  qbPixelMap p = new qbPixelMap_;
  p->width = width;
  p->height = height;
  p->format = format;

  p->pixel_size = format == qbPixelFormat::QB_PIXEL_FORMAT_RGBA8 ? 4 : 0;
  p->size = width * height * p->pixel_size;
  if (p->size && pixels) {
    p->pixels = (void*)new char[p->size];
    memcpy(p->pixels, pixels, p->size);
  } else {
    p->pixels = nullptr;
  }
  p->row_size =  p->pixel_size * p->width;

  return p;
}

void qb_pixelmap_destroy(qbPixelMap* pixels) {
  delete[] (char*)((*pixels)->pixels);
  delete *pixels;
  *pixels = nullptr;
}

uint32_t qb_pixelmap_width(qbPixelMap pixels) {
  return pixels->width;
}

uint32_t qb_pixelmap_height(qbPixelMap pixels) {
  return pixels->height;
}

void* qb_pixelmap_pixels(qbPixelMap pixels) {
  if (!pixels->pixels) {
    pixels->pixels = (void*)new char[pixels->size];
  }
  return pixels->pixels;
}

qbResult qb_pixelmap_drawto(qbPixelMap src, qbPixelMap dest, glm::vec2 src_rect, glm::vec2 dest_rect) {
  return QB_OK;
}

size_t qb_pixelmap_size(qbPixelMap pixels) {
  return pixels->size;
}

size_t qb_pixelmap_rowsize(qbPixelMap pixels) {
  return pixels->row_size;
}

size_t qb_pixelmap_pixelsize(qbPixelMap pixels) {
  return pixels->pixel_size;
}

qbPixelFormat qb_pixelmap_format(qbPixelMap pixels) {
  return pixels->format;
}

void qb_renderpipeline_create(qbRenderPipeline* pipeline) {
  *pipeline = new qbRenderPipeline_;
}

void qb_renderpipeline_destroy(qbRenderPipeline* pipeline) {
  delete *pipeline;
  *pipeline = nullptr;
}

void qb_renderpipeline_run(qbRenderPipeline render_pipeline, qbRenderEvent event) {
  for (qbRenderPass pass : render_pipeline->render_passes) {
    if (pass->frame_buffer) {
      glBindFramebuffer(GL_FRAMEBUFFER, pass->frame_buffer->id);
    } else {
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    GLenum clear = 0;
    if (pass->clear.attachments & QB_COLOR_ATTACHMENT) {
      glClearColor(pass->clear.color.r,
                   pass->clear.color.g,
                   pass->clear.color.b,
                   pass->clear.color.a);
      clear |= GL_COLOR_BUFFER_BIT;
    }
    if (pass->clear.attachments & QB_DEPTH_ATTACHMENT) {
      glClearDepth(pass->clear.depth);
      clear |= GL_DEPTH_BUFFER_BIT;
    }
    if (pass->clear.attachments & QB_STENCIL_ATTACHMENT) {
      glClearStencil(pass->clear.stencil);
      clear |= GL_STENCIL_BUFFER_BIT;
    }
    glClear(clear);
  }

  for (qbRenderPass pass : render_pipeline->render_passes) {
    qb_renderpass_draw(pass);
  }
}

void qb_renderpipeline_append(qbRenderPipeline pipeline, qbRenderPass pass) {
  pipeline->render_passes.push_back(pass);
}

void qb_renderpipeline_prepend(qbRenderPipeline pipeline, qbRenderPass pass) {
  pipeline->render_passes.insert(pipeline->render_passes.begin(), pass);
}

size_t qb_renderpipeline_passes(qbRenderPipeline pipeline, qbRenderPass** passes) {
  *passes = pipeline->render_passes.data();
  return pipeline->render_passes.size();
}

void qb_renderpipeline_update(qbRenderPipeline pipeline, size_t count, qbRenderPass* pass) {
  pipeline->render_passes.resize(count);
  for (size_t i = 0; i < count; ++i) {
    pipeline->render_passes[i] = pass[i];
  }
}

void qb_renderpipeline_remove(qbRenderPipeline pipeline, qbRenderPass pass) {
  auto found = std::find(pipeline->render_passes.begin(), pipeline->render_passes.end(), pass);
  if (found != pipeline->render_passes.end()) {
    pipeline->render_passes.erase(found);
  }
}

void qb_shadermodule_create(qbShaderModule* shader_ref, qbShaderModuleAttr attr) {
  qbShaderModule module = *shader_ref = new qbShaderModule_();
  qb_shader_load(&module->shader, "", attr->vs, attr->fs, attr->gs);

  module->resources_count = attr->resources_count;
  module->resources = new qbShaderResourceInfo_[module->resources_count];
  memcpy(module->resources, attr->resources, module->resources_count * sizeof(qbShaderResourceInfo_));

  uint32_t program = (uint32_t)qb_shader_getid(module->shader);

  for (auto i = 0; i < module->resources_count; ++i) {
    qbShaderResourceInfo attr = module->resources + i;
    if (attr->resource_type == QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER) {
      int32_t block_index = glGetUniformBlockIndex(program, attr->name);
      glUniformBlockBinding(program, block_index, attr->binding);
      CHECK_GL();
    }
  }
}

void qb_shadermodule_destroy(qbShaderModule* shader) {
  delete (*shader)->resources;
  delete (*shader)->uniform_bindings;
  delete (*shader)->uniforms;
  delete (*shader)->sampler_bindings;
  delete (*shader)->samplers;
  delete *shader;
  *shader = nullptr;
}

void qb_shadermodule_attachuniforms(qbShaderModule module, size_t count, uint32_t bindings[], qbGpuBuffer uniforms[]) {
  uint32_t program = (uint32_t)qb_shader_getid(module->shader);

  module->uniforms_count = count;
  module->uniform_bindings = new uint32_t[module->uniforms_count];
  module->uniforms = new qbGpuBuffer[module->uniforms_count];

  memcpy(module->uniform_bindings, bindings, count * sizeof(uint32_t));
  memcpy(module->uniforms, uniforms, count * sizeof(qbGpuBuffer));
}

void qb_shadermodule_attachsamplers(qbShaderModule module, size_t count, uint32_t bindings[], qbImageSampler samplers[]) {
  module->samplers_count = count;
  module->sampler_bindings = new uint32_t[module->samplers_count];
  module->samplers = new qbImageSampler[module->samplers_count];

  memcpy(module->sampler_bindings, bindings, module->samplers_count * sizeof(uint32_t));
  memcpy(module->samplers, samplers, module->samplers_count * sizeof(qbImageSampler));

  for (size_t i = 0; i < module->samplers_count; ++i) {
    qbImageSampler sampler = module->samplers[i];
    uint32_t binding = module->sampler_bindings[i];

    qbShaderResourceInfo resource = nullptr;
    for (size_t j = 0; j < module->resources_count; ++j) {
      if (module->resources[j].binding == binding &&
          module->resources[j].resource_type == QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER) {
        resource = module->resources + j;
        break;
      }
    }

    sampler->name = resource->name;
  }
}

void qb_gpubuffer_create(qbGpuBuffer* buffer_ref, qbGpuBufferAttr attr) {
  *buffer_ref = new qbGpuBuffer_;
  qbGpuBuffer buffer = *buffer_ref;
  buffer->size = attr->size;
  buffer->elem_size = attr->elem_size;
  buffer->buffer_type = attr->buffer_type;
  buffer->sharing_type = attr->sharing_type;
  buffer->name = attr->name;
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

void qb_meshbuffer_create(qbMeshBuffer* buffer_ref, qbMeshBufferAttr attr) {
  *buffer_ref = new qbMeshBuffer_;
  qbMeshBuffer buffer = *buffer_ref;

  buffer->bindings_count = attr->bindings_count;
  buffer->bindings = new qbBufferBinding_[buffer->bindings_count];
  memcpy(buffer->bindings, attr->bindings, attr->bindings_count * sizeof(qbBufferBinding_));

  buffer->attributes_count = attr->attributes_count;
  buffer->attributes = new qbVertexAttribute_[buffer->attributes_count];
  memcpy(buffer->attributes, attr->attributes, buffer->attributes_count * sizeof(qbVertexAttribute_));

  glGenVertexArrays(1, &buffer->id);
  buffer->vertices = new qbGpuBuffer[buffer->bindings_count];

  buffer->uniforms_count = 0;
  buffer->uniforms = nullptr;
  buffer->uniform_bindings = nullptr;

  buffer->images_count = 0;
  buffer->sampler_bindings = nullptr;
  buffer->images = nullptr;
  buffer->instance_count = attr->instance_count;
  buffer->name = attr->name;
  buffer->ext = attr->ext;

  CHECK_GL();
}

void qb_meshbuffer_destroy(qbMeshBuffer* buffer_ref) {
  glDeleteVertexArrays(1, &(*buffer_ref)->id);
  delete[](*buffer_ref)->bindings;
  delete[](*buffer_ref)->attributes;
  delete[](*buffer_ref)->vertices;
  delete[](*buffer_ref)->sampler_bindings;
  delete[](*buffer_ref)->images;
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
  if (buffer->images_count != 0) {
    FATAL("qbMeshBuffer already has attached images. Use qb_meshbuffer_updateimages to change attached images");
  }
  buffer->images_count = count;
  buffer->sampler_bindings = new uint32_t[buffer->images_count];
  buffer->images = new qbImage[buffer->images_count];

  memcpy(buffer->sampler_bindings, bindings, buffer->images_count * sizeof(uint32_t));
  memcpy(buffer->images, images, buffer->images_count * sizeof(qbImage));
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

void qb_meshbuffer_attachvertices(qbMeshBuffer buffer, qbGpuBuffer vertices[]) {
  glBindVertexArray(buffer->id);

  for (size_t i = 0; i < buffer->bindings_count; ++i) {
    qbBufferBinding binding = buffer->bindings + i;
    buffer->vertices[i] = vertices[binding->binding];
    if (!vertices[binding->binding]) {
      FATAL("Vertices[ " << binding->binding << " ] is null");
    }

    qbGpuBuffer gpu_buffer = buffer->vertices[i];
    GLenum target = TranslateQbGpuBufferTypeToOpenGl(gpu_buffer->buffer_type);
    glBindBuffer(target, gpu_buffer->id);
    CHECK_GL();

    for (size_t j = 0; j < buffer->attributes_count; ++j) {
      qbVertexAttribute vattr = buffer->attributes + j;
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
}

void qb_meshbuffer_attachindices(qbMeshBuffer buffer, qbGpuBuffer indices) {
  glBindVertexArray(buffer->id);
  glGenBuffers(1, &indices->id);
  glBindBuffer(TranslateQbGpuBufferTypeToOpenGl(indices->buffer_type), indices->id);
  glBufferData(TranslateQbGpuBufferTypeToOpenGl(indices->buffer_type), indices->size, indices->data, GL_DYNAMIC_DRAW);

  if (!indices) {
    FATAL("Indices are null");
  }

  buffer->indices = indices;
  CHECK_GL();
}

void qb_meshbuffer_render(qbMeshBuffer buffer, uint32_t bindings[]) {
  // Bind textures.
  for (size_t i = 0; i < buffer->images_count; ++i) {
    glActiveTexture((GLenum)(GL_TEXTURE0 + i));

    uint32_t image_id = 0;
    uint32_t module_binding = bindings[i];

    for (size_t j = 0; j < buffer->images_count; ++j) {
      uint32_t buffer_binding = buffer->sampler_bindings[j];
      if (module_binding == buffer_binding) {
        image_id = buffer->images[j]->id;
        break;
      }
    }

    glBindTexture(GL_TEXTURE_2D, (GLuint)image_id);
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
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices->id);
  if (buffer->instance_count == 0) {
    glDrawElements(GL_TRIANGLES, (GLsizei)(indices->size / indices->elem_size), GL_UNSIGNED_INT, nullptr);
  } else {
    glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)(indices->size / indices->elem_size), GL_UNSIGNED_INT, nullptr, buffer->instance_count);
  }
  CHECK_GL();
}

size_t qb_meshbuffer_vertices(qbMeshBuffer buffer, qbGpuBuffer** vertices) {
  *vertices = buffer->vertices;
  return buffer->bindings_count;
}

void qb_meshbuffer_indices(qbMeshBuffer buffer, qbGpuBuffer* indices) {
  *indices = buffer->indices;
}

size_t qb_meshbuffer_images(qbMeshBuffer buffer, qbImage** images) {
  *images = buffer->images;
  return buffer->images_count;
}

size_t qb_meshbuffer_uniforms(qbMeshBuffer buffer, qbGpuBuffer** uniforms) {
  *uniforms = buffer->uniforms;
  return buffer->uniforms_count;
}

void qb_renderpass_create(qbRenderPass* render_pass_ref, qbRenderPassAttr attr) {
  *render_pass_ref = new qbRenderPass_;
  qbRenderPass render_pass = *render_pass_ref;

  render_pass->frame_buffer = attr->frame_buffer;
  render_pass->viewport = attr->viewport;
  render_pass->viewport_scale = attr->viewport_scale;
  render_pass->shader_program = attr->shader;
  render_pass->clear = attr->clear;
  render_pass->name = attr->name;
  render_pass->ext = attr->ext;

  render_pass->attributes_count = attr->attributes_count;
  render_pass->attributes = new qbVertexAttribute_[render_pass->attributes_count];
  memcpy((void*)render_pass->attributes, attr->attributes, render_pass->attributes_count * sizeof(qbVertexAttribute_));

  render_pass->bindings_count = attr->bindings_count;
  render_pass->bindings = new qbBufferBinding_[render_pass->bindings_count];
  memcpy(render_pass->bindings, attr->bindings, render_pass->bindings_count * sizeof(qbBufferBinding_));
}

void qb_renderpass_destroy(qbRenderPass* render_pass) {
  delete[](*render_pass)->attributes;
  delete[](*render_pass)->bindings;

  delete *render_pass;
  *render_pass = nullptr;
}

qbFrameBuffer* qb_renderpass_frame(qbRenderPass render_pass) {
  return &render_pass->frame_buffer;
}

size_t qb_renderpass_bindings(qbRenderPass render_pass, qbBufferBinding* bindings) {
  *bindings = render_pass->bindings;
  return render_pass->bindings_count;
}

size_t qb_renderpass_attributes(qbRenderPass render_pass, qbVertexAttribute* attributes) {
  *attributes = render_pass->attributes;
  return render_pass->attributes_count;
}

size_t qb_renderpass_buffers(qbRenderPass render_pass, qbMeshBuffer** buffers) {
  *buffers = render_pass->draw_buffers.data();
  return render_pass->draw_buffers.size();
}

size_t qb_renderpass_resources(qbRenderPass render_pass, qbShaderResourceInfo* resources) {
  *resources = render_pass->shader_program->resources;
  return render_pass->shader_program->resources_count;
}

size_t qb_renderpass_uniforms(qbRenderPass render_pass, uint32_t** bindings, qbGpuBuffer** uniforms) {
  return render_pass->shader_program->samplers_count;
}

size_t qb_renderpass_samplers(qbRenderPass render_pass, uint32_t** bindings, qbImageSampler** samplers) {
  *bindings = render_pass->shader_program->sampler_bindings;
  *samplers = render_pass->shader_program->samplers;
  return render_pass->shader_program->samplers_count;
}

const char* qb_renderpass_name(qbRenderPass render_pass) {
  return render_pass->name;
}

qbRenderExt qb_renderpass_ext(qbRenderPass render_pass) {
  return render_pass->ext;
}

void qb_renderpass_append(qbRenderPass render_pass, qbMeshBuffer buffer) {
  render_pass->draw_buffers.push_back(buffer);
}

void qb_renderpass_prepend(qbRenderPass render_pass, qbMeshBuffer buffer) {
  render_pass->draw_buffers.insert(render_pass->draw_buffers.begin(), buffer);
}

void qb_renderpass_remove(qbRenderPass render_pass, qbMeshBuffer buffer) {
  auto& it = std::find(render_pass->draw_buffers.begin(), render_pass->draw_buffers.end(), buffer);
  if (it != render_pass->draw_buffers.end()) {
    render_pass->draw_buffers.erase(it);
  }
}

void qb_renderpass_update(qbRenderPass render_pass, size_t count, qbMeshBuffer* buffers) {
  render_pass->draw_buffers.resize(count);
  for (size_t i = 0; i < count; ++i) {
    render_pass->draw_buffers[i] = buffers[i];
  }
}

void qb_renderpass_draw(qbRenderPass render_pass) {
  if (render_pass->frame_buffer) {
    glBindFramebuffer(GL_FRAMEBUFFER, render_pass->frame_buffer->id);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  } else {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
  }

  CHECK_GL();
  qbShaderModule module = render_pass->shader_program;
  glViewport(render_pass->viewport.x, render_pass->viewport.y,
             GLsizei(render_pass->viewport.p * render_pass->viewport_scale),
             GLsizei(render_pass->viewport.q * render_pass->viewport_scale));

  // Render passes are defined by having different shaders. When shaders are
  // re-linked (glUseProgram), it resets the uniform block binding and bound
  // uniform blocks. Re-do all block bindings and re-bind uniform blocks.
  qb_shader_use(module->shader);
  uint32_t program = (uint32_t)qb_shader_getid(module->shader);
  for (size_t i = 0; i < module->resources_count; ++i) {
    qbShaderResourceInfo attr = module->resources + i;
    if (attr->resource_type == QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER) {
      int32_t block_index = glGetUniformBlockIndex(program, attr->name);
      glUniformBlockBinding(program, block_index, attr->binding);
      for (size_t i = 0; i < module->uniforms_count; ++i) {
        uint32_t binding = module->uniform_bindings[i];
        qbGpuBuffer uniform = module->uniforms[i];
        if (binding == attr->binding) {
          glBindBufferBase(GL_UNIFORM_BUFFER, binding, uniform->id);
          break;
        }
      }
      CHECK_GL();
    }
  }

  for (size_t index = 0; index < module->samplers_count; ++index) {
    qbImageSampler sampler = module->samplers[index];
    glActiveTexture((GLenum)(GL_TEXTURE0 + index));
    glUniform1i(glGetUniformLocation(program, sampler->name), (GLint)index);
    glBindSampler((GLuint)index, (GLuint)sampler->id);
    CHECK_GL();
  }
  
  for (auto buffer : render_pass->draw_buffers) {
    qb_meshbuffer_render(buffer, render_pass->shader_program->sampler_bindings);
  }
}

void qb_imagesampler_create(qbImageSampler* sampler_ref, qbImageSamplerAttr attr) {
  //https://orangepalantir.org/topicspace/show/84
  qbImageSampler sampler = *sampler_ref = new qbImageSampler_;
  sampler->attr = *attr;
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
  glDeleteSamplers(1, &(*sampler_ref)->id);
  delete *sampler_ref;
  *sampler_ref = nullptr;
}

void qb_image_create(qbImage* image_ref, qbImageAttr attr, qbPixelMap pixel_map) {
  qbImage image = *image_ref = new qbImage_;
  image->type = attr->type;
  image->format = pixel_map->format;
  image->name = attr->name;
  image->ext = attr->ext;

  // Load the image from the file into SDL's surface representation
  GLenum image_type = TranslateQbImageTypeToOpenGl(image->type);
  
  glGenTextures(1, &image->id);
  glBindTexture(image_type, image->id);

  int stored_alignment;
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &stored_alignment);

  qbRenderExt ext = image->ext;
  while (ext) {
    if (ext->name && strcmp(ext->name, "qbOglPixelAlignmentExt_") == 0) {
      qbPixelAlignmentOglExt_* u_ext = (qbPixelAlignmentOglExt_*)ext;
      glPixelStorei(GL_UNPACK_ALIGNMENT, u_ext->alignment);
      break;
    }
    ext = ext->next;
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
  }
  if (attr->generate_mipmaps) {
    glGenerateMipmap(image_type);
  }

  glPixelStorei(GL_UNPACK_ALIGNMENT, stored_alignment);
  CHECK_GL();
}

void qb_image_raw(qbImage* image_ref, qbImageAttr attr, qbPixelFormat format, uint32_t width, uint32_t height, void* pixels) {
  qbImage image = *image_ref = new qbImage_;
  image->type = attr->type;
  image->format = format;
  image->name = attr->name;
  image->ext = attr->ext;

  // Load the image from the file into SDL's surface representation
  GLenum image_type = TranslateQbImageTypeToOpenGl(image->type);

  glGenTextures(1, &image->id);
  glBindTexture(image_type, image->id);

  int stored_alignment;
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &stored_alignment);

  qbRenderExt ext = image->ext;
  while (ext) {
    if (ext->name && strcmp(ext->name, "qbOglPixelAlignmentExt_") == 0) {
      qbPixelAlignmentOglExt_* u_ext = (qbPixelAlignmentOglExt_*)ext;
      glPixelStorei(GL_UNPACK_ALIGNMENT, u_ext->alignment);
      break;
    }
    ext = ext->next;
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
  delete *image;
  *image = nullptr;
}

const char* qb_image_name(qbImage image) {
  return image->name;
}

void qb_image_load(qbImage* image_ref, qbImageAttr attr, const char* file) {
  qbImage image = *image_ref = new qbImage_;

  // Load the image from the file into SDL's surface representation
  SDL_Surface* surf = SDL_LoadBMP(file);
  if (!surf) {
    std::cout << "Could not load texture " << file << std::endl;
  }
  GLenum image_type = TranslateQbImageTypeToOpenGl(attr->type);
  
  glGenTextures(1, &image->id);
  glBindTexture(image_type, image->id);

  if (image_type == GL_TEXTURE_1D) {
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA8, surf->w * surf->h, 0, GL_BGRA, GL_UNSIGNED_BYTE, surf->pixels);
  } else if (image_type == GL_TEXTURE_2D) {
    if (surf->format->BytesPerPixel == 3 && surf->format->Amask == 0) {
      glTexImage2D(image_type, 0, GL_RGBA8, surf->w, surf->h, 0, GL_RGB, GL_UNSIGNED_BYTE, surf->pixels);
    } else {
      glTexImage2D(image_type, 0, GL_RGBA8, surf->w, surf->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, surf->pixels);
    }
  }
  if (attr->generate_mipmaps) {
    glGenerateMipmap(image_type);
  }

  SDL_FreeSurface(surf);
  CHECK_GL();
}

void qb_image_update(qbImage image, glm::vec3 offset, glm::vec3 sizes, void* data) {
  GLenum image_type = TranslateQbImageTypeToOpenGl(image->type);
  GLenum format = TranslateQbPixelFormatToOpenGl(image->format);
  GLenum type = TranslateQbPixelFormatToOpenGlSize(image->format);

  if (image_type == GL_TEXTURE_1D) {
    glTexSubImage1D(image_type, 0, offset.x, sizes.x, format, type, data);
  } else if (image_type == GL_TEXTURE_2D) {
    glTexSubImage2D(image_type, 0, offset.x, offset.y, sizes.x, sizes.y, format, type, data);
  } else if (image_type == GL_TEXTURE_3D) {
    glTexSubImage3D(image_type, 0, offset.x, offset.y, offset.z, sizes.x, sizes.y, sizes.z, format, type, data);
  }

  CHECK_GL();
}

void qb_framebuffer_create(qbFrameBuffer* frame_buffer, qbFrameBufferAttr attr) {
  *frame_buffer = new qbFrameBuffer_;

  uint32_t frame_buffer_id = 0;
  glGenFramebuffers(1, &frame_buffer_id);
  (*frame_buffer)->id = frame_buffer_id;
  CHECK_GL();
  glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer_id);
  CHECK_GL();

  if (attr->attachments & QB_COLOR_ATTACHMENT) {
    qbImageAttr_ image_attr = {};
    image_attr.type = QB_IMAGE_TYPE_2D;
    qb_image_raw(&(*frame_buffer)->render_target, &image_attr,
                 qbPixelFormat::QB_PIXEL_FORMAT_RGBA8, attr->width, attr->height, nullptr);
    glBindTexture(GL_TEXTURE_2D, (*frame_buffer)->render_target->id);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, (*frame_buffer)->render_target->id, 0);
    CHECK_GL();
  }

  if (attr->attachments & QB_DEPTH_STENCIL_ATTACHMENT) {
    qbImageAttr_ image_attr = {};
    image_attr.type = QB_IMAGE_TYPE_2D;
    qb_image_raw(&(*frame_buffer)->depthstencil_target, &image_attr,
                 qbPixelFormat::QB_PIXEL_FORMAT_D24_S8, attr->width, attr->height, nullptr);
    glBindTexture(GL_TEXTURE_2D, (*frame_buffer)->depthstencil_target->id);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, (*frame_buffer)->depthstencil_target->id, 0);
    CHECK_GL();
  } else {
    if (attr->attachments & QB_DEPTH_ATTACHMENT) {
      qbImageAttr_ image_attr = {};
      image_attr.type = QB_IMAGE_TYPE_2D;
      qb_image_raw(&(*frame_buffer)->depth_target, &image_attr,
                   qbPixelFormat::QB_PIXEL_FORMAT_D32, attr->width, attr->height, nullptr);
      glBindTexture(GL_TEXTURE_2D, (*frame_buffer)->depth_target->id);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, (*frame_buffer)->depth_target->id, 0);
      CHECK_GL();
    }
    if (attr->attachments & QB_STENCIL_ATTACHMENT) {
      qbImageAttr_ image_attr = {};
      image_attr.type = QB_IMAGE_TYPE_2D;
      qb_image_raw(&(*frame_buffer)->stencil_target, &image_attr,
                   qbPixelFormat::QB_PIXEL_FORMAT_S8, attr->width, attr->height, nullptr);
      glBindTexture(GL_TEXTURE_2D, (*frame_buffer)->stencil_target->id);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, (*frame_buffer)->stencil_target->id, 0);
      CHECK_GL();
    }
  }

  if (attr->attachments & QB_COLOR_ATTACHMENT) {
    GLenum drawBuffers[1] = { GL_COLOR_ATTACHMENT0 + attr->color_binding };
    glDrawBuffers(1, drawBuffers);
    CHECK_GL();
  }

  GLenum result = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  CHECK_GL();
  if (result != GL_FRAMEBUFFER_COMPLETE) {
    FATAL("Error creating FBO: " << result);
  }
  CHECK_GL();
}

void qb_framebuffer_destroy(qbFrameBuffer* frame_buffer) {
  delete (*frame_buffer)->render_target;
  delete (*frame_buffer)->depth_target;
  delete (*frame_buffer)->stencil_target;
  delete (*frame_buffer)->depthstencil_target;
  delete *frame_buffer;
  *frame_buffer = nullptr;
}

qbImage qb_framebuffer_rendertarget(qbFrameBuffer frame_buffer) {
  return frame_buffer->render_target;
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