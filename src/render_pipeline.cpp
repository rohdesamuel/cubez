#define GLM_GTC_matrix_transform
#include <cubez/cubez.h>

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <cubez/render_pipeline.h>
#include <cubez/common.h>
#include <algorithm>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <cubez/render.h>
#include "shader.h"
#include <cubez/utils.h>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include "stb_image.h"

#ifdef _DEBUG
#define CHECK_GL()  {if (GLenum err = glGetError()) FATAL(gluErrorString(err)) }
#else
#define CHECK_GL()
#endif   

#define WITH_MODULE(module, fn, ...) (module)->render_interface.fn((module)->impl, __VA_ARGS__)

typedef struct qbRenderPipeline_ {
  std::vector<qbRenderPass> render_passes;
  qbRenderPass present_pass;
} qbRenderPipeline_;

typedef struct qbFrameBuffer_ {
  uint32_t id;
  qbImage render_target;
  qbImage depth_target;
  qbImage stencil_target;
  qbImage depthstencil_target;

  qbFrameBufferAttr_ attr;
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

  qbGeometryDescriptor_ descriptor;

  size_t instance_count;
} qbMeshBuffer_;

typedef struct qbShaderModule_ {
  ShaderProgram* shader;

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

  std::vector<qbRenderGroup> groups;

  qbGeometryDescriptor_ supported_geometry;

  qbShaderModule shader_program;

  glm::vec4 viewport;
  float viewport_scale;

  qbClearValue_ clear;

} qbRenderPass_;

typedef struct qbRenderGroup_ {
  std::vector<qbMeshBuffer> meshes;

  std::vector<uint32_t> uniform_bindings;
  std::vector<qbGpuBuffer> uniforms;

  std::vector<uint32_t> sampler_bindings;
  std::vector<qbImage> images;

} qbRenderGroup_, *qbRenderGroup;

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

void qb_renderpipeline_create(qbRenderPipeline* pipeline, qbRenderPipelineAttr pipeline_attr) {
  *pipeline = new qbRenderPipeline_;
  
  // Create the presentation pass.
  {
    qbVertexAttribute_ attributes[2] = {};
    {
      qbVertexAttribute_* attr = attributes;
      attr->binding = 0;
      attr->location = 0;

      attr->count = 2;
      attr->type = QB_VERTEX_ATTRIB_TYPE_FLOAT;
      attr->normalized = false;
      attr->offset = (void*)0;
    }
    {
      qbVertexAttribute_* attr = attributes + 1;
      attr->binding = 0;
      attr->location = 1;

      attr->count = 2;
      attr->type = QB_VERTEX_ATTRIB_TYPE_FLOAT;
      attr->normalized = false;
      attr->offset = (void*)(2 * sizeof(float));
    }

    qbShaderModule shader_module;
    {
      qbShaderResourceInfo_ sampler_attr = {};
      sampler_attr.binding = 0;
      sampler_attr.resource_type = QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER;
      sampler_attr.stages = QB_SHADER_STAGE_FRAGMENT;
      sampler_attr.name = "tex_sampler";

      qbShaderModuleAttr_ attr = {};
      attr.vs = "resources/texture.vs";
      attr.fs = "resources/texture.fs";

      attr.resources = &sampler_attr;
      attr.resources_count = 1;

      qb_shadermodule_create(&shader_module, &attr);

      qbImageSampler sampler;
      {
        qbImageSamplerAttr_ attr = {};
        attr.image_type = QB_IMAGE_TYPE_2D;
        attr.min_filter = QB_FILTER_TYPE_LINEAR;
        attr.mag_filter = QB_FILTER_TYPE_LINEAR;
        qb_imagesampler_create(&sampler, &attr);
      }

      uint32_t bindings[] = { 0 };
      qbImageSampler samplers[] = { sampler };
      qb_shadermodule_attachsamplers(shader_module, 1, bindings, samplers);
    }

    qbBufferBinding_ binding;
    binding.binding = 0;
    binding.stride = 4 * sizeof(float);
    binding.input_rate = QB_VERTEX_INPUT_RATE_VERTEX;
    {
      qbRenderPassAttr_ attr = {};
      attr.frame_buffer = nullptr;

      attr.supported_geometry.bindings = &binding;
      attr.supported_geometry.bindings_count = 1;

      attr.supported_geometry.attributes = attributes;
      attr.supported_geometry.attributes_count = 2;

      attr.shader = shader_module;

      attr.viewport = pipeline_attr->viewport;
      attr.viewport_scale = pipeline_attr->viewport_scale;

      qbClearValue_ clear;
      clear.attachments = QB_COLOR_ATTACHMENT;
      clear.color = { 0.0, 0.0, 0.0, 1.0 };
      attr.clear = clear;

      qb_renderpass_create(&(*pipeline)->present_pass, &attr);
    }

    qbGpuBuffer gpu_buffers[2];
    {
      qbGpuBufferAttr_ attr = {};
      float vertices[] = {
        // Positions   // UVs
        -1.0, -1.0,    0.0, 0.0,
        -1.0,  1.0,    0.0, 1.0,
         1.0,  1.0,    1.0, 1.0,
         1.0, -1.0,    1.0, 0.0
      };

      attr.buffer_type = QB_GPU_BUFFER_TYPE_VERTEX;
      attr.data = vertices;
      attr.size = sizeof(vertices);
      attr.elem_size = sizeof(float);
      qb_gpubuffer_create(gpu_buffers, &attr);
    }
    {
      qbGpuBufferAttr_ attr = {};
      int indices[] = {
        0, 1, 2,
        0, 2, 3,
      };
      attr.buffer_type = QB_GPU_BUFFER_TYPE_INDEX;
      attr.data = indices;
      attr.size = sizeof(indices);
      attr.elem_size = sizeof(int);
      qb_gpubuffer_create(gpu_buffers + 1, &attr);
    }

    qbMeshBuffer draw_buff;
    {
      qbMeshBufferAttr_ attr = {};
      attr.descriptor = (*pipeline)->present_pass->supported_geometry;

      qb_meshbuffer_create(&draw_buff, &attr);
    }

    qbGpuBuffer vertices[] = { gpu_buffers[0] };
    qb_meshbuffer_attachvertices(draw_buff, vertices);
    qb_meshbuffer_attachindices(draw_buff, gpu_buffers[1]);

    uint32_t image_bindings[] = { 0 };
    qbImage images[] = { nullptr };
    qb_meshbuffer_attachimages(draw_buff, 1, image_bindings, images);

    qbRenderGroup group;
    {
      qbRenderGroupAttr_ attr = {};
      qbMeshBuffer meshes[] = { draw_buff };
      attr.mesh_count = 1;
      attr.meshes = meshes;
      qb_rendergroup_create(&group, &attr);
    }
    qb_renderpass_append((*pipeline)->present_pass, group);
  }
}

void qb_renderpipeline_destroy(qbRenderPipeline* pipeline) {
  delete *pipeline;
  *pipeline = nullptr;
}

void qb_renderpipeline_render(qbRenderPipeline render_pipeline, qbRenderEvent event) {
  for (qbRenderPass pass : render_pipeline->render_passes) {
    qb_renderpass_clear(pass);
  }

  for (qbRenderPass pass : render_pipeline->render_passes) {
    qb_renderpass_draw(pass);
  }
}

void qb_renderpipeline_present(qbRenderPipeline render_pipeline, qbFrameBuffer frame_buffer,
                               qbRenderEvent event) {
  render_pipeline->present_pass->groups[0]->meshes[0]->images[0] = frame_buffer->render_target;
  qb_renderpass_clear(render_pipeline->present_pass);
  qb_renderpass_draw(render_pipeline->present_pass);
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

size_t qb_renderpipeline_remove(qbRenderPipeline pipeline, qbRenderPass pass) {
  auto found = std::find(pipeline->render_passes.begin(), pipeline->render_passes.end(), pass);
  if (found != pipeline->render_passes.end()) {
    pipeline->render_passes.erase(found);
    return 1;
  }
  return 0;
}

void qb_shadermodule_create(qbShaderModule* shader_ref, qbShaderModuleAttr attr) {
  qbShaderModule module = *shader_ref = new qbShaderModule_();
  std::string vs = attr->vs ? attr->vs : "";
  std::string fs = attr->fs ? attr->fs : "";
  std::string gs = attr->gs ? attr->gs : "";
  module->shader = new ShaderProgram(ShaderProgram::load_from_file(vs, fs, gs));

  module->resources_count = attr->resources_count;
  module->resources = new qbShaderResourceInfo_[module->resources_count];
  memcpy(module->resources, attr->resources, module->resources_count * sizeof(qbShaderResourceInfo_));

  uint32_t program = (uint32_t)module->shader->id();

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
  uint32_t program = (uint32_t)module->shader->id();

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

    if (!sampler->name) {
      sampler->name = STRDUP(resource->name);
    }
  }
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

  buffer->images_count = 0;
  buffer->sampler_bindings = nullptr;
  buffer->images = nullptr;
  buffer->instance_count = attr->instance_count;
  buffer->name = STRDUP(attr->name);
  buffer->ext = attr->ext;

  CHECK_GL();
}

void qb_meshbuffer_destroy(qbMeshBuffer* buffer_ref) {
  free((void*)(*buffer_ref)->name);
  glDeleteVertexArrays(1, &(*buffer_ref)->id);

  delete[](*buffer_ref)->descriptor.bindings;
  delete[](*buffer_ref)->descriptor.attributes;
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
}

void qb_meshbuffer_attachindices(qbMeshBuffer buffer, qbGpuBuffer indices) {
  glBindVertexArray(buffer->id);
  //glGenBuffers(1, &indices->id);
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
  return buffer->descriptor.bindings_count;
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
  render_pass->name = STRDUP(attr->name);
  render_pass->ext = attr->ext;


  qbGeometryDescriptor descriptor = &render_pass->supported_geometry;
  descriptor->attributes_count = attr->supported_geometry.attributes_count;
  descriptor->attributes = new qbVertexAttribute_[descriptor->attributes_count];
  memcpy((void*)descriptor->attributes, attr->supported_geometry.attributes,
         descriptor->attributes_count * sizeof(qbVertexAttribute_));

  descriptor->bindings_count = attr->supported_geometry.bindings_count;
  descriptor->bindings = new qbBufferBinding_[descriptor->bindings_count];
  memcpy(descriptor->bindings, attr->supported_geometry.bindings,
         descriptor->bindings_count * sizeof(qbBufferBinding_));
}

void qb_renderpass_destroy(qbRenderPass* render_pass) {
  free((void*)(*render_pass)->name);
  delete[](*render_pass)->supported_geometry.attributes;
  delete[](*render_pass)->supported_geometry.bindings;

  delete *render_pass;
  *render_pass = nullptr;
}

qbFrameBuffer* qb_renderpass_frame(qbRenderPass render_pass) {
  return &render_pass->frame_buffer;
}

void qb_renderpass_setframe(qbRenderPass render_pass, qbFrameBuffer fbo) {
  render_pass->frame_buffer = fbo;
}

qbGeometryDescriptor qb_renderpass_supportedgeometry(qbRenderPass render_pass) {
  return &render_pass->supported_geometry;
}

size_t qb_renderpass_bindings(qbRenderPass render_pass, qbBufferBinding* bindings) {
  *bindings = render_pass->supported_geometry.bindings;
  return render_pass->supported_geometry.bindings_count;
}

size_t qb_renderpass_attributes(qbRenderPass render_pass, qbVertexAttribute* attributes) {
  *attributes = render_pass->supported_geometry.attributes;
  return render_pass->supported_geometry.attributes_count;
}

size_t qb_renderpass_groups(qbRenderPass render_pass, qbRenderGroup** groups) {
  *groups = render_pass->groups.data();
  return render_pass->groups.size();
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

void qb_renderpass_append(qbRenderPass render_pass, qbRenderGroup group) {
  render_pass->groups.push_back(group);
}

void qb_renderpass_prepend(qbRenderPass render_pass, qbRenderGroup group) {
  render_pass->groups.insert(render_pass->groups.begin(), group);
}

size_t qb_renderpass_remove(qbRenderPass render_pass, qbRenderGroup group) {
  auto& it = std::find(render_pass->groups.begin(), render_pass->groups.end(), group);
  if (it != render_pass->groups.end()) {
    render_pass->groups.erase(it);
    return 1;
  }
  return 0;
}

void qb_renderpass_update(qbRenderPass render_pass, size_t count, qbRenderGroup* buffers) {
  render_pass->groups.resize(0);
  for (size_t i = 0; i < count; ++i) {
    render_pass->groups[i] = buffers[i];
  }
}

void qb_renderpass_clear(qbRenderPass render_pass) {
  if (render_pass->frame_buffer) {
    glBindFramebuffer(GL_FRAMEBUFFER, render_pass->frame_buffer->id);
  } else {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  GLenum clear = 0;
  if (render_pass->clear.attachments & QB_COLOR_ATTACHMENT) {
    glClearColor(render_pass->clear.color.r,
                 render_pass->clear.color.g,
                 render_pass->clear.color.b,
                 render_pass->clear.color.a);
    clear |= GL_COLOR_BUFFER_BIT;
  }
  if (render_pass->clear.attachments & QB_DEPTH_ATTACHMENT) {
    glClearDepth(render_pass->clear.depth);
    clear |= GL_DEPTH_BUFFER_BIT;
  }
  if (render_pass->clear.attachments & QB_STENCIL_ATTACHMENT) {
    glClearStencil(render_pass->clear.stencil);
    clear |= GL_STENCIL_BUFFER_BIT;
  }
  glClear(clear);
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
  module->shader->use();
  uint32_t program = (uint32_t)module->shader->id();
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

  for (auto group : render_pass->groups) {
    // Bind textures.
    for (size_t i = 0; i < group->images.size(); ++i) {
      uint32_t image_id = 0;
      uint32_t offset = 0;
      uint32_t buffer_binding = group->sampler_bindings[i];
      for (size_t j = 0; j < render_pass->shader_program->samplers_count; ++j) {
        uint32_t module_binding = render_pass->shader_program->sampler_bindings[j];
        if (module_binding == buffer_binding) {
          image_id = group->images[i]->id;
          offset = j;
          break;
        }
      }
      
      glActiveTexture((GLenum)(GL_TEXTURE0 + offset));
      glBindTexture(GL_TEXTURE_2D, (GLuint)image_id);
    }

    // Bind uniform blocks for this group.
    for (size_t i = 0; i < group->uniforms.size(); ++i) {
      qbGpuBuffer uniform = group->uniforms[i];
      uint32_t binding = group->uniform_bindings[i];
      glBindBufferBase(GL_UNIFORM_BUFFER, binding, uniform->id);
    }
    for (auto mesh : group->meshes) {
      qb_meshbuffer_render(mesh, render_pass->shader_program->sampler_bindings);
    }
  }
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

  qbRenderExt ext = qb_renderext_find(image->ext, "qbPixelAlignmentOglExt_");
  if (ext) {
    qbPixelAlignmentOglExt_* u_ext = (qbPixelAlignmentOglExt_*)ext;
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
  image->name = STRDUP(attr->name);
  image->ext = attr->ext;

  // Load the image from the file into SDL's surface representation
  GLenum image_type = TranslateQbImageTypeToOpenGl(image->type);

  glGenTextures(1, &image->id);
  glBindTexture(image_type, image->id);

  int stored_alignment;
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &stored_alignment);

  qbRenderExt ext = qb_renderext_find(image->ext, "qbPixelAlignmentOglExt_");
  if (ext) {
    qbPixelAlignmentOglExt_* u_ext = (qbPixelAlignmentOglExt_*)ext;
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
  delete *image;
  *image = nullptr;
}

const char* qb_image_name(qbImage image) {
  return image->name;
}

void qb_image_load(qbImage* image_ref, qbImageAttr attr, const char* file) {
  qbImage image = *image_ref = new qbImage_;

  // Load the image from the file into SDL's surface representation
  int w, h, n;
  unsigned char* pixels = stbi_load(file, &w, &h, &n, 0);

  if (!pixels) {
    std::cout << "Could not load texture " << file << ": " << stbi_failure_reason() << std::endl;
    return;
  }

  GLenum image_type = TranslateQbImageTypeToOpenGl(attr->type);
  
  glGenTextures(1, &image->id);
  glBindTexture(image_type, image->id);

  if (image_type == GL_TEXTURE_1D) {
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA8, w * h, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
  } else if (image_type == GL_TEXTURE_2D) {
    if (n == 1) {
      glTexImage2D(image_type, 0, GL_RED, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, pixels);
    } else if (n == 2) {
      glTexImage2D(image_type, 0, GL_RG, w, h, 0, GL_RG, GL_UNSIGNED_BYTE, pixels);
    } else if (n == 3) {
      glTexImage2D(image_type, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    } else if (n == 4) {
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

void qb_image_update(qbImage image, glm::ivec3 offset, glm::ivec3 sizes, void* data) {
  GLenum image_type = TranslateQbImageTypeToOpenGl(image->type);
  GLenum format = TranslateQbPixelFormatToOpenGl(image->format);
  GLenum type = TranslateQbPixelFormatToOpenGlSize(image->format);

  glBindTexture(image_type, image->id);

  int stored_alignment;
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &stored_alignment);
  qbRenderExt ext = qb_renderext_find(image->ext, "qbPixelAlignmentOglExt_");
  if (ext) {
    qbPixelAlignmentOglExt_* u_ext = (qbPixelAlignmentOglExt_*)ext;
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

void qb_framebuffer_init(qbFrameBuffer frame_buffer, qbFrameBufferAttr attr) {
  uint32_t frame_buffer_id = 0;
  glGenFramebuffers(1, &frame_buffer_id);
  frame_buffer->id = frame_buffer_id;
  frame_buffer->attr = *attr;
  CHECK_GL();
  glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer_id);
  CHECK_GL();

  if (attr->attachments & QB_COLOR_ATTACHMENT) {
    qbImageAttr_ image_attr = {};
    image_attr.type = QB_IMAGE_TYPE_2D;
    qb_image_raw(&frame_buffer->render_target, &image_attr,
                 qbPixelFormat::QB_PIXEL_FORMAT_RGBA8, attr->width, attr->height, nullptr);
    glBindTexture(GL_TEXTURE_2D, frame_buffer->render_target->id);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, frame_buffer->render_target->id, 0);
    CHECK_GL();
  }

  if (attr->attachments & QB_DEPTH_STENCIL_ATTACHMENT) {
    qbImageAttr_ image_attr = {};
    image_attr.type = QB_IMAGE_TYPE_2D;
    qb_image_raw(&frame_buffer->depthstencil_target, &image_attr,
                 qbPixelFormat::QB_PIXEL_FORMAT_D24_S8, attr->width, attr->height, nullptr);
    glBindTexture(GL_TEXTURE_2D, frame_buffer->depthstencil_target->id);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, frame_buffer->depthstencil_target->id, 0);
    CHECK_GL();
  } else {
    if (attr->attachments & QB_DEPTH_ATTACHMENT) {
      qbImageAttr_ image_attr = {};
      image_attr.type = QB_IMAGE_TYPE_2D;
      qb_image_raw(&frame_buffer->depth_target, &image_attr,
                   qbPixelFormat::QB_PIXEL_FORMAT_D32, attr->width, attr->height, nullptr);
      glBindTexture(GL_TEXTURE_2D, frame_buffer->depth_target->id);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, frame_buffer->depth_target->id, 0);
      CHECK_GL();
    }
    if (attr->attachments & QB_STENCIL_ATTACHMENT) {
      qbImageAttr_ image_attr = {};
      image_attr.type = QB_IMAGE_TYPE_2D;
      qb_image_raw(&frame_buffer->stencil_target, &image_attr,
                   qbPixelFormat::QB_PIXEL_FORMAT_S8, attr->width, attr->height, nullptr);
      glBindTexture(GL_TEXTURE_2D, frame_buffer->stencil_target->id);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, frame_buffer->stencil_target->id, 0);
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

void qb_framebuffer_clear(qbFrameBuffer frame_buffer) {
  delete frame_buffer->render_target;
  delete frame_buffer->depth_target;
  delete frame_buffer->stencil_target;
  delete frame_buffer->depthstencil_target;
}

void qb_framebuffer_create(qbFrameBuffer* frame_buffer, qbFrameBufferAttr attr) {
  *frame_buffer = new qbFrameBuffer_;
  qb_framebuffer_init(*frame_buffer, attr);
}

void qb_framebuffer_destroy(qbFrameBuffer* frame_buffer) {
  qb_framebuffer_clear(*frame_buffer);
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

void qb_framebuffer_resize(qbFrameBuffer frame_buffer, uint32_t width, uint32_t height) {
  qb_framebuffer_clear(frame_buffer);
  frame_buffer->attr.width = width;
  frame_buffer->attr.height = height;
  qb_framebuffer_init(frame_buffer, &frame_buffer->attr);
}

uint32_t qb_framebuffer_width(qbFrameBuffer frame_buffer) {
  return frame_buffer->attr.width;
}

uint32_t qb_framebuffer_height(qbFrameBuffer frame_buffer) {
  return frame_buffer->attr.height;
}

qbRenderExt qb_renderext_find(qbRenderExt extensions, const char* ext_name) {
  while (extensions && extensions->name && strcmp(extensions->name, ext_name) != 0) {
    extensions = extensions->next;
  }
  return extensions;
}

void qb_rendergroup_create(qbRenderGroup* group, qbRenderGroupAttr attr) {
  *group = new qbRenderGroup_{};

  for (size_t i = 0; i < attr->mesh_count; ++i) {
    (*group)->meshes.push_back(attr->meshes[i]);
  }
}
void qb_rendergroup_destroy(qbRenderGroup* group) {
  delete *group;
  *group = nullptr;
}

void qb_rendergroup_attachimages(qbRenderGroup buffer, size_t count, uint32_t bindings[], qbImage images[]) {
  if (buffer->images.empty()) {
    for (uint32_t i = 0; i < count; ++i) {
      buffer->images.push_back(images[i]);
      buffer->sampler_bindings.push_back(bindings[i]);
    }
  } else {
    for (uint32_t i = 0; i < count; ++i) {
      qbImage image = images[i];
      uint32_t binding = bindings[i];

      int32_t match = -1;
      for (auto b : buffer->sampler_bindings) {
        if (b == binding) {
          match = b;
          break;
        }
      }
      if (match == -1) {
        buffer->images.push_back(image);
        buffer->sampler_bindings.push_back(binding);
      } else {
        buffer->images[match] = image;
      }
    }
  }
}

void qb_rendergroup_attachuniforms(qbRenderGroup buffer, size_t count, uint32_t bindings[], qbGpuBuffer uniforms[]) {
  if (buffer->uniforms.empty()) {
    for (uint32_t i = 0; i < count; ++i) {
      buffer->uniforms.push_back(uniforms[i]);
      buffer->uniform_bindings.push_back(bindings[i]);
    }
  } else {
    for (uint32_t i = 0; i < count; ++i) {
      qbGpuBuffer uniform = uniforms[i];
      uint32_t binding = bindings[i];

      int32_t match = -1;
      for (auto b : buffer->uniform_bindings) {
        if (b == binding) {
          match = b;
          break;
        }
      }
      if (match == -1) {
        buffer->uniforms.push_back(uniform);
        buffer->uniform_bindings.push_back(binding);
      } else {
        buffer->uniforms[match] = uniform;
      }
    }
  }
}

void qb_rendergroup_adduniform(qbRenderGroup buffer, qbGpuBuffer uniform, uint32_t binding) {
  buffer->uniforms.push_back(uniform);
  buffer->uniform_bindings.push_back(binding);
}

void qb_rendergroup_addimage(qbRenderGroup buffer, qbImage image, uint32_t binding) {
  buffer->images.push_back(image);
  buffer->sampler_bindings.push_back(binding);
}

size_t qb_rendergroup_meshes(qbRenderGroup group, qbMeshBuffer** buffers) {
  *buffers = group->meshes.data();
  return group->meshes.size();
}

void qb_rendergroup_append(qbRenderGroup group, qbMeshBuffer buffer) {
  group->meshes.push_back(buffer);
}

void qb_rendergroup_prepend(qbRenderGroup group, qbMeshBuffer buffer) {
  group->meshes.insert(group->meshes.begin(), buffer);
}

size_t qb_rendergroup_remove(qbRenderGroup group, qbMeshBuffer buffer) {
  auto it = std::find(group->meshes.begin(), group->meshes.end(), buffer);
  if (it != group->meshes.end()) {
    group->meshes.erase(it);
    return 1;
  }
  return 0;
}

void qb_rendergroup_update(qbRenderGroup group, size_t count, qbMeshBuffer* buffers) {
  group->meshes.resize(0);
  for (size_t i = 0; i < count; ++i) {
    group->meshes.push_back(buffers[i]);
  }
}

size_t qb_rendergroup_images(qbRenderGroup buffer, qbImage** images) {
  *images = buffer->images.data();
  return buffer->images.size();
}

size_t qb_rendergroup_uniforms(qbRenderGroup buffer, qbGpuBuffer** uniforms) {
  *uniforms = buffer->uniforms.data();
  return buffer->uniforms.size();
}

qbImage qb_rendergroup_findimage(qbRenderGroup buffer, qbImage image) {
  auto it = std::find(buffer->images.begin(), buffer->images.end(), image);
  if (it == buffer->images.end()) {
    return nullptr;
  }
  return *it;
}

qbImage qb_rendergroup_findimage_byname(qbRenderGroup buffer, const char* name) {
  std::string str_name(name);
  auto it = std::find_if(buffer->images.begin(), buffer->images.end(), [&str_name](qbImage image) {
    return std::string(image->name) == str_name;
  });
  if (it == buffer->images.end()) {
    return nullptr;
  }
  return *it;
}

qbImage qb_rendergroup_findimage_bybinding(qbRenderGroup buffer, uint32_t binding) {
  auto it = std::find(buffer->sampler_bindings.begin(), buffer->sampler_bindings.end(), binding);
  if (it == buffer->sampler_bindings.end()) {
    return nullptr;
  }
  auto diff = it - buffer->sampler_bindings.begin();
  return *(buffer->images.begin() + diff);
}

size_t qb_rendergroup_removeimage(qbRenderGroup buffer, qbImage image) {
  auto& it = std::find(buffer->images.begin(), buffer->images.end(), image);
  if (it != buffer->images.end()) {
    auto diff = it - buffer->images.begin();
    buffer->images.erase(it);
    buffer->sampler_bindings.erase(buffer->sampler_bindings.begin() + diff);
    return 1;
  }
  return 0;
}

size_t qb_rendergroup_removeimage_byname(qbRenderGroup buffer, const char* name) {
  if (!name) {
    return 0;
  }

  size_t num_removed = 0;
  for (auto it = buffer->images.begin(); it != buffer->images.end();) {
    qbImage image = *it;
    if (std::string(image->name) == std::string(name)) {
      auto diff = it - buffer->images.begin();
      it = buffer->images.erase(it);
      buffer->sampler_bindings.erase(buffer->sampler_bindings.begin() + diff);
      ++num_removed;
    } else {
      ++it;
    }
  }
  return num_removed;
}

size_t qb_rendergroup_removeimage_bybinding(qbRenderGroup buffer, uint32_t binding) {
  auto& it = std::find(buffer->sampler_bindings.begin(), buffer->sampler_bindings.end(), binding);
  if (it != buffer->sampler_bindings.end()) {
    auto diff = it - buffer->sampler_bindings.begin();
    buffer->sampler_bindings.erase(it);
    buffer->images.erase(buffer->images.begin() + diff);
    return 1;
  }
  return 0;
}

qbGpuBuffer qb_rendergroup_finduniform(qbRenderGroup buffer, qbGpuBuffer uniform) {
  auto it = std::find(buffer->uniforms.begin(), buffer->uniforms.end(), uniform);
  if (it == buffer->uniforms.end()) {
    return nullptr;
  }
  return *it;
}

qbGpuBuffer qb_rendergroup_finduniform_byname(qbRenderGroup buffer, const char* name) {
  std::string str_name(name);
  auto it = std::find_if(buffer->uniforms.begin(), buffer->uniforms.end(), [&str_name](qbGpuBuffer uniform) {
    return std::string(uniform->name) == str_name;
  });
  if (it == buffer->uniforms.end()) {
    return nullptr;
  }
  return *it;
}

qbGpuBuffer qb_rendergroup_finduniform_bybinding(qbRenderGroup buffer, uint32_t binding) {
  auto it = std::find(buffer->uniform_bindings.begin(), buffer->uniform_bindings.end(), binding);
  if (it == buffer->uniform_bindings.end()) {
    return nullptr;
  }
  auto diff = it - buffer->uniform_bindings.begin();
  return *(buffer->uniforms.begin() + diff);
}

size_t qb_rendergroup_removeuniform(qbRenderGroup buffer, qbGpuBuffer uniform) {
  auto& it = std::find(buffer->uniforms.begin(), buffer->uniforms.end(), uniform);
  if (it != buffer->uniforms.end()) {
    auto diff = it - buffer->uniforms.begin();
    buffer->uniforms.erase(it);
    buffer->uniform_bindings.erase(buffer->uniform_bindings.begin() + diff);
    return 1;
  }
  return 0;
}

size_t qb_rendergroup_removeuniform_byname(qbRenderGroup buffer, const char* name) {
  if (!name) {
    return 0;
  }

  size_t num_removed = 0;
  for (auto it = buffer->uniforms.begin(); it != buffer->uniforms.end();) {
    qbGpuBuffer uniform = *it;
    if (std::string(uniform->name) == std::string(name)) {
      auto diff = it - buffer->uniforms.begin();
      it = buffer->uniforms.erase(it);
      buffer->uniform_bindings.erase(buffer->uniform_bindings.begin() + diff);
      ++num_removed;
    } else {
      ++it;
    }
  }
  return num_removed;
}

size_t qb_rendergroup_removeuniform_bybinding(qbRenderGroup buffer, uint32_t binding) {
  auto& it = std::find(buffer->uniform_bindings.begin(), buffer->uniform_bindings.end(), binding);
  if (it != buffer->uniform_bindings.end()) {
    auto diff = it - buffer->uniform_bindings.begin();
    buffer->uniform_bindings.erase(it);
    buffer->uniforms.erase(buffer->uniforms.begin() + diff);
    return 1;
  }
  return 0;
}