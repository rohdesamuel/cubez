#include "render.h"
#include <cubez/common.h>

#define GL3_PROTOTYPES 1

#include <iostream>
#include <GL/glew.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "render.h"
#include "opengl_render_module.h"
#include "shader.h"

#ifdef _DEBUG
#define CHECK_GL()  {if (GLenum err = glGetError()) FATAL(gluErrorString(err)) }
#else
#define CHECK_GL()
#endif   

#define WITH_MODULE(module, fn, ...) (module)->render_interface.fn((module)->impl, __VA_ARGS__)

typedef struct qbPixelMap_ {
  uint32_t width;
  uint32_t height;

  qbPixelFormat format;
  void* pixel;
  size_t size;
  size_t row_size;
  size_t pixel_size;

} qbPixelMap_;

typedef struct qbRenderModule_ {
  qbRenderInterface_ render_interface;
  qbRenderImpl impl;

  std::vector<qbRenderPass> render_passes;
} qbRenderModule_;

typedef struct qbRenderPipeline_ {
  std::vector<qbRenderPass> render_passes;
} qbRenderPipeline_;

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

}

qbPixelMap qb_pixelmap_create(uint32_t width, uint32_t height, qbPixelFormat format) {
  qbPixelMap p = new qbPixelMap_;
  p->width = width;
  p->height = height;
  p->format = format;

  p->pixel_size = format == qbPixelFormat::QB_PIXEL_FORMAT_RGBA8 ? 4 : 0;
  p->size = width * height * p->pixel_size;
  p->pixel = (void*)new char[p->size];
  p->row_size =  p->pixel_size * p->width;

  return p;
}

qbResult qb_pixelmap_destroy(qbPixelMap* pixels) {
  delete[] (char*)((*pixels)->pixel);
  delete *pixels;
  *pixels = nullptr;
  return QB_OK;
}

uint32_t qb_pixelmap_width(qbPixelMap pixels) {
  return pixels->width;
}

uint32_t qb_pixelmap_height(qbPixelMap pixels) {
  return pixels->height;
}

void* qb_pixelmap_pixels(qbPixelMap pixels) {
  return pixels->pixel;
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

qbResult qb_rendermodule_create(
  qbRenderModule* module,
  qbRenderInterface render_interface,
  qbRenderImpl impl,
  uint32_t viewport_width,
  uint32_t viewport_height,
  float scale) {

  *module = new qbRenderModule_;
  //(*module)->render_interface = *render_interface;
  (*module)->impl = impl;

  return QB_OK;
}

void      qb_renderpipeline_create(qbRenderPipeline* pipeline,
                                 uint32_t viewport_width,
                                 uint32_t viewport_height,
                                 float scale) {
  *pipeline = new qbRenderPipeline_;
}

qbResult qb_rendermodule_destroy(qbRenderModule* module) {
  (*module)->render_interface.ondestroy((*module)->impl);
  delete *module;
  *module = nullptr;
  return QB_OK;
}

qbResult qb_rendermodule_resize(qbRenderModule module, uint32_t width, uint32_t height) {
  return QB_OK;
}

void qb_rendermodule_lockstate(qbRenderModule module) {
  WITH_MODULE(module, lockstate);
}

void qb_rendermodule_unlockstate(qbRenderModule module) {
  WITH_MODULE(module, unlockstate);
}

void qb_renderpipeline_run(qbRenderPipeline render_pipeline, qbRenderEvent event) {
  for (qbRenderPass pass : render_pipeline->render_passes) {
    qb_rendermodule_drawrenderpass(pass);
  }
}

void qb_shadermodule_create(qbShaderModule* shader_ref, qbShaderModuleAttr attr) {
  qbShaderModule module = *shader_ref = new qbShaderModule_();
  qb_shader_load(&module->shader, "", attr->vs, attr->fs);

  module->resources_count = attr->resources_count;
  module->resources = new qbShaderResourceAttr_[module->resources_count];
  memcpy(module->resources, attr->resources, module->resources_count * sizeof(qbShaderResourceAttr_));
}

void qb_shadermodule_attachuniforms(qbShaderModule module, uint32_t count, uint32_t bindings[], qbGpuBuffer uniforms[]) {
  uint32_t program = qb_shader_getid(module->shader);

  for (auto i = 0; i < count; ++i) {
    qbGpuBuffer uniform = uniforms[i];
    uint32_t binding = bindings[i];
    qbShaderResourceAttr attr = nullptr;
    for (auto j = 0; j < module->resources_count; ++j) {
      if (module->resources[j].binding == binding) {
        attr = module->resources + j;
        break;
      }
    }
    if (attr == nullptr) {
      FATAL("qbShaderResourceAttr buffer for binding " << binding << " not found.");
    }
    int32_t block_index = glGetUniformBlockIndex(program, attr->name);
    glUniformBlockBinding(program, block_index, attr->binding);
    glBindBufferBase(GL_UNIFORM_BUFFER, attr->binding, uniform->id);
    CHECK_GL();
  }
}

void qb_shadermodule_attachsamplers(qbShaderModule module, uint32_t count, uint32_t bindings[], qbImageSampler samplers[]) {
  module->samplers_count = count;

  module->sampler_bindings = new uint32_t[module->samplers_count];
  module->samplers = new qbImageSampler[module->samplers_count];

  memcpy(module->sampler_bindings, bindings, module->samplers_count * sizeof(uint32_t));
  memcpy(module->samplers, samplers, module->samplers_count * sizeof(qbImageSampler));

  for (size_t i = 0; i < count; ++i) {
    qbImageSampler sampler = samplers[i];
    sampler->binding = bindings[i];

    qbShaderResourceAttr resource = nullptr;
    for (size_t j = 0; j < module->resources_count; ++j) {
      if (module->resources[j].binding == sampler->binding &&
          module->resources[j].resource_type == QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER) {
        resource = module->resources + j;
        break;
      }
    }

    if (resource == nullptr) {
      FATAL("qbShaderResourceAttr for binding " << sampler->binding << " not found.");
    }

    sampler->name = resource->name;
  }
}

void qb_drawbuffer_attachimages(qbDrawBuffer buffer, size_t count, uint32_t bindings[], qbImage images[]) {
  if (count < buffer->render_pass->shader_program->samplers_count) {
    FATAL("Not enough images given. There are " << buffer->render_pass->shader_program->samplers_count - count << " more samplers than images");
  } else if (count > buffer->render_pass->shader_program->samplers_count) {
    FATAL("Too many images given. There are " << count - buffer->render_pass->shader_program->samplers_count << " more images than samplers");
  }
  for (size_t i = 0; i < count; ++i) {
    uint32_t binding = bindings[i];
    for (size_t j = 0; j < count; ++j) {
      uint32_t sampler_binding = buffer->render_pass->shader_program->sampler_bindings[j];
      if (binding == sampler_binding) {
        buffer->images[j] = images[i];
        break;
      }
    }
  }
}

void qb_drawbuffer_attachuniforms(qbDrawBuffer buffer, uint32_t count, uint32_t bindings[], qbGpuBuffer uniforms[]) {
  uint32_t program = qb_shader_getid(buffer->render_pass->shader_program->shader);
  for (auto i = 0; i < buffer->render_pass->shader_program->resources_count; ++i) {
    qbShaderResourceAttr attr = buffer->render_pass->shader_program->resources + i;

    if (attr->resource_type == QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER) {
      qbGpuBuffer uniform = nullptr;
      for (auto j = 0; j < count; ++j) {
        if (attr->binding == bindings[j]) {
          uniform = uniforms[j];
          break;
        }
      }
      if (uniform == nullptr) {
        FATAL("Uniform buffer for binding " << attr->binding << " not found.");
      }
      int32_t block_index = glGetUniformBlockIndex(program, attr->name);
      glUniformBlockBinding(program, block_index, attr->binding);
      glBindBufferBase(GL_UNIFORM_BUFFER, attr->binding, uniform->id);
    }
  }
  CHECK_GL();
}


void qb_gpubuffer_create(qbGpuBuffer* buffer_ref, qbGpuBufferAttr attr) {
  *buffer_ref = new qbGpuBuffer_;
  qbGpuBuffer buffer = *buffer_ref;
  buffer->size = attr->size;
  buffer->elem_size = attr->elem_size;
  buffer->buffer_type = attr->buffer_type;
  buffer->sharing_type = attr->sharing_type;

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

void qb_drawbuffer_attachvertices(qbDrawBuffer draw_buffer, qbGpuBuffer vertices[]) {
  qbRenderPass render_pass = draw_buffer->render_pass;

  glBindVertexArray(draw_buffer->id);

  for (size_t i = 0; i < draw_buffer->render_pass->bindings_count; ++i) {
    qbBufferBinding binding = draw_buffer->render_pass->bindings + i;
    draw_buffer->vertices[i] = vertices[binding->binding];

    qbGpuBuffer buffer = draw_buffer->vertices[i];
    GLenum target = TranslateQbGpuBufferTypeToOpenGl(buffer->buffer_type);
    glBindBuffer(target, buffer->id);
    CHECK_GL();

    for (size_t j = 0; j < render_pass->attributes_count; ++j) {
      qbVertexAttribute vattr = render_pass->attributes + j;
      glEnableVertexAttribArray(vattr->location);
      glVertexAttribPointer(vattr->location, vattr->size,
                            TranslateQbVertexAttribTypeToOpenGl(vattr->type),
                            vattr->normalized, binding->stride, vattr->offset);
      CHECK_GL();
    }
  }
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

void qb_drawbuffer_attachindices(qbDrawBuffer draw_buffer, qbGpuBuffer indices) {
  glBindVertexArray(draw_buffer->id);
  glGenBuffers(1, &indices->id);
  glBindBuffer(TranslateQbGpuBufferTypeToOpenGl(indices->buffer_type), indices->id);
  glBufferData(TranslateQbGpuBufferTypeToOpenGl(indices->buffer_type), indices->size, indices->data, GL_DYNAMIC_DRAW);
  draw_buffer->indices = indices;
  CHECK_GL();
}

void qb_drawbuffer_create(qbDrawBuffer* draw_buffer_ref, qbDrawBufferAttr attr, qbRenderPass render_pass) {
  *draw_buffer_ref = new qbDrawBuffer_;
  qbDrawBuffer draw_buffer = *draw_buffer_ref;

  glGenVertexArrays(1, &draw_buffer->id);
  draw_buffer->vertices = new qbGpuBuffer[render_pass->bindings_count];

  uint32_t image_sampler_count = 0;
  for (size_t i = 0; i < render_pass->shader_program->resources_count; ++i) {
    if (render_pass->shader_program->resources[i].resource_type == QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER) {
      ++image_sampler_count;
    }
  }

  
  draw_buffer->images_count = image_sampler_count;
  draw_buffer->images = new qbImage[draw_buffer->images_count];

  draw_buffer->render_pass = render_pass;

  render_pass->draw_buffers.push_back(draw_buffer);
  
  CHECK_GL();
}

void qb_renderpass_create(qbRenderPass* render_pass_ref, qbRenderPassAttr attr, qbRenderPipeline pipeline) {
  *render_pass_ref = new qbRenderPass_;
  qbRenderPass render_pass = *render_pass_ref;

  render_pass->frame_buffer = attr->frame_buffer;
  render_pass->viewport = attr->viewport;
  render_pass->viewport_scale = attr->viewport_scale;
  render_pass->shader_program = attr->shader_program;
  
  render_pass->attributes_count = attr->attributes_count;
  render_pass->attributes = new qbVertexAttribute_[render_pass->attributes_count];
  memcpy((void*)render_pass->attributes, attr->attributes, render_pass->attributes_count * sizeof(qbVertexAttribute_));

  render_pass->bindings_count = attr->bindings_count;
  render_pass->bindings = new qbBufferBinding_[render_pass->bindings_count];
  memcpy(render_pass->bindings, attr->bindings, render_pass->bindings_count * sizeof(qbBufferBinding_));
  
  // Opengl specific
  /*
  uint32_t render_pass_id;
  glGenVertexArrays(1, &render_pass_id);
  glBindVertexArray(render_pass_id);
  CHECK_GL();

  for (size_t i = 0; i < attr->gpu_buffers_count; ++i) {
    qbGpuBuffer buffer = render_pass->gpu_buffers + i;

    glGenBuffers(1, &buffer->id);
    glBindBuffer(buffer->buffer_type, buffer->id);
    glBufferData(buffer->buffer_type, buffer->size, buffer->data, buffer->usage_type);

    CHECK_GL();
  }

  for (size_t attribute_pos = 0; attribute_pos < attr->attributes_count; ++attribute_pos) {
    qbVertexAttribute vattr = attr->attributes + attribute_pos;
    glEnableVertexAttribArray(attribute_pos);
    glVertexAttribPointer(attribute_pos, vattr->size, vattr->type,
                          vattr->normalized, vattr->stride, vattr->offset);    
  }
  CHECK_GL();

  glBindVertexArray(0);
  render_pass->id = render_pass_id;
  render_pass->shader_program = attr->shader_program;
  */
  // End opengl specific
  pipeline->render_passes.push_back(render_pass);
}

void qb_drawbuffer_render(qbDrawBuffer draw_buffer) {
  glBindVertexArray(draw_buffer->id);
  for (size_t i = 0; i < draw_buffer->images_count; ++i) {
    glActiveTexture(GL_TEXTURE0 + i);
    glBindTexture(GL_TEXTURE_2D, draw_buffer->images[i]->id);
  }
  qbGpuBuffer indices = draw_buffer->indices;
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices->id);
  glDrawElements(GL_TRIANGLES, indices->size / indices->elem_size, GL_UNSIGNED_INT, nullptr);
  CHECK_GL();
}

void qb_rendermodule_drawrenderpass(qbRenderPass render_pass) {
  if (render_pass->frame_buffer) {
    glBindFramebuffer(GL_FRAMEBUFFER, render_pass->frame_buffer->id);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    //glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glClearColor(0.0f, 0.1f, 0.1f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  } else {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(0.0f, 1.0f, 1.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
  }

  CHECK_GL();
  glViewport(render_pass->viewport.x, render_pass->viewport.y,
             GLsizei(render_pass->viewport.p * render_pass->viewport_scale),
             GLsizei(render_pass->viewport.q * render_pass->viewport_scale));

  qb_shader_use(render_pass->shader_program->shader);
  uint32_t program = qb_shader_getid(render_pass->shader_program->shader);
  for (size_t i = 0; i < render_pass->shader_program->samplers_count; ++i) {
    qbImageSampler sampler = render_pass->shader_program->samplers[i];

    glActiveTexture(GL_TEXTURE0 + i);
    glUniform1i(glGetUniformLocation(program, sampler->name), i);
    glBindSampler(i, sampler->id);
    CHECK_GL();
  }
  /*for (size_t i = 0; i < render_pass->shader_program->resources_count; ++i) {
    qbShaderResourceAttr attr = render_pass->shader_program->resources + i;
    uint32_t program = qb_shader_getid(render_pass->shader_program->shader);
    if (attr->resource_type == QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER) {
      glActiveTexture(GL_TEXTURE0 + active_textures);
      glUniform1i(glGetUniformLocation(program, attr->name), active_textures);
      GLenum texture_type = TranslateQbImageTypeToOpenGl(render_pass->shader_program->samplers[active_textures]->attr.image_type);
      glBindSampler(active_textures, render_pass->shader_program->samplers[active_textures]->id);
      CHECK_GL();
      ++active_textures;
    }
  }*/

  CHECK_GL();
  for (auto draw_buffer : render_pass->draw_buffers) {
    qb_drawbuffer_render(draw_buffer);
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

void qb_image_create(qbImage* image_ref, qbImageAttr attr) {
  qbImage image = *image_ref = new qbImage_;
  image->id = 0;
}

void qb_image_create(qbImage* image, qbPixelMap pixels) {
  *image = new qbImage_;

  GLuint tex_id;
  glGenTextures(1, &tex_id);
  glBindTexture(GL_TEXTURE_2D, tex_id);

  if (qb_pixelmap_format(pixels) == qbPixelFormat::QB_PIXEL_FORMAT_RGBA8) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, qb_pixelmap_width(pixels), qb_pixelmap_height(pixels), 0,
                 GL_BGRA, GL_UNSIGNED_BYTE, qb_pixelmap_pixels(pixels));
  } else {
    FATAL("Unhandled texture format: " << qb_pixelmap_format(pixels));
  }

  CHECK_GL();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  CHECK_GL();
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  CHECK_GL();
  //glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  //glPixelStorei(GL_UNPACK_ROW_LENGTH, qb_pixelmap_rowsize(pixels) / qb_pixelmap_pixelsize(pixels));
  CHECK_GL();


  CHECK_GL();
  glGenerateMipmap(GL_TEXTURE_2D);
  CHECK_GL();
  (*image)->id = tex_id;

  //return WITH_MODULE(module, createtexture, pixel_map);
}

void qb_image_load(qbImage* image_ref, qbImageAttr attr, const char* file) {
  qbImage image = *image_ref = new qbImage_;
  image->pixels = nullptr;

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

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
  
  SDL_FreeSurface(surf);
  CHECK_GL();
}

void qb_rendermodule_updatetexture(qbRenderModule module,
                                   uint32_t texture_id,
                                   qbPixelMap pixel_map) {
  WITH_MODULE(module, updatetexture, texture_id, pixel_map);
}

void qb_rendermodule_usetexture(qbRenderModule module,
                                uint32_t texture_id,
                                uint32_t index,
                                uint32_t target) {
  WITH_MODULE(module, usetexture, texture_id, index, target);
}

void qb_rendermodule_destroytexture(qbRenderModule module,
                                    uint32_t texture_id) {
  WITH_MODULE(module, destroytexture, texture_id);
}

void qb_framebuffer_create(qbFrameBuffer* frame_buffer, qbFrameBufferAttr attr) {
  *frame_buffer = new qbFrameBuffer_;

  uint32_t frame_buffer_id = 0;
  glGenFramebuffers(1, &frame_buffer_id);
  CHECK_GL();
  glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer_id);
  CHECK_GL();

  qbPixelMap pixels = qb_pixelmap_create(attr->width,
                                      attr->height,
                                      qbPixelFormat::QB_PIXEL_FORMAT_RGBA8);

  qb_image_create(&(*frame_buffer)->render_target, pixels);
  uint32_t texture_id = (*frame_buffer)->render_target->id;

  glBindTexture(GL_TEXTURE_2D, texture_id);
  CHECK_GL();
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id, 0);
  CHECK_GL();

  // Create the render buffer for depth testing.
  unsigned int rbo;
  glGenRenderbuffers(1, &rbo);
  glBindRenderbuffer(GL_RENDERBUFFER, rbo);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, attr->width, attr->height);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);

  GLenum drawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
  glDrawBuffers(1, drawBuffers);
  CHECK_GL();

  GLenum result = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (result != GL_FRAMEBUFFER_COMPLETE) {
    FATAL("Error creating FBO: " << result);
  }
  CHECK_GL();

  (*frame_buffer)->id = frame_buffer_id;
  //return WITH_MODULE(module, createframebuffer, frame_buffer, attr);
#if 0
  void qb_framebuffer_create(qbFrameBuffer* frame_buffer, qbFrameBufferAttr attr) {
    *frame_buffer = new qbFrameBuffer_;

    uint32_t frame_buffer_id = 0;
    glGenFramebuffers(1, &frame_buffer_id);
    CHECK_GL();
    glBindFramebuffer(GL_FRAMEBUFFER, frame_buffer_id);
    CHECK_GL();

    GLenum draw_buffers[1] = { GL_COLOR_ATTACHMENT0 };

    if (attr->attachments & QB_COLOR_ATTACHMENT) {
      uint32_t texture;
      glGenTextures(1, &texture);
      glBindTexture(GL_TEXTURE_2D, texture);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, attr->width, attr->height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glBindTexture(GL_TEXTURE_2D, texture);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
      (*frame_buffer)->render_target = texture;
      CHECK_GL();
    }

    if (attr->attachments & QB_DEPTH_STENCIL_ATTACHMENT) {
      uint32_t texture;
      glGenTextures(1, &texture);
      glBindTexture(GL_TEXTURE_2D, texture);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, attr->width, attr->height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glBindTexture(GL_TEXTURE_2D, texture);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, texture, 0);
      CHECK_GL();
    } else {
      if (attr->attachments & QB_DEPTH_ATTACHMENT) {
        uint32_t texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, attr->width, attr->height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, texture);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texture, 0);
        CHECK_GL();
      }
      if (attr->attachments & QB_DEPTH_ATTACHMENT) {
        uint32_t texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_STENCIL_INDEX, attr->width, attr->height, 0, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, texture);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, texture, 0);
        CHECK_GL();
      }
    }


    glDrawBuffers(1, draw_buffers);
    CHECK_GL();

    GLenum result = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (result != GL_FRAMEBUFFER_COMPLETE) {
      FATAL("Error creating FBO: " << result);
    }
    CHECK_GL();

    (*frame_buffer)->id = frame_buffer_id;
    //return WITH_MODULE(module, createframebuffer, frame_buffer, attr);
  }
#endif
}

void qb_rendermodule_bindframebuffer(qbRenderModule module,
                                      uint32_t frame_buffer_id) {
  WITH_MODULE(module, bindframebuffer, frame_buffer_id);
}

void qb_rendermodule_setframebufferviewport(qbRenderModule module,
                                             uint32_t frame_buffer_id,
                                             uint32_t width,
                                             uint32_t height) {
  WITH_MODULE(module, setframebufferviewport, frame_buffer_id, width, height);
}

void qb_rendermodule_clearframebuffer(qbRenderModule module,
                                       uint32_t frame_buffer_id) {
  WITH_MODULE(module, clearframebuffer, frame_buffer_id);
}

void qb_rendermodule_destroyframebuffer(qbRenderModule module,
                                         uint32_t frame_buffer_id) {
  WITH_MODULE(module, destroyframebuffer, frame_buffer_id);
}

uint32_t qb_rendermodule_creategeometry(qbRenderModule module,
                                        const qbVertexBuffer vertices,
                                        const qbIndexBuffer indices,
                                        const qbGpuBuffer* buffers, size_t buffer_size,
                                        const qbVertexAttribute* attributes, size_t attr_size) {
  /*uint32_t id = WITH_MODULE(module, nextgeometryid);
  WITH_MODULE(module, creategeometry,
              id,
              vertices,
              indices,
              buffers, buffer_size,
              attributes, attr_size);*/
  return 0;
}

void qb_rendermodule_updategeometry(qbRenderModule module,
                                    uint32_t geometry_id,
                                    const qbVertexBuffer vertices,
                                    const qbIndexBuffer indices,
                                    const qbGpuBuffer* buffers, size_t buffer_size) {
  WITH_MODULE(module, updategeometry,
              geometry_id,
              vertices,
              indices,
              buffers, buffer_size);
}

void qb_rendermodule_destroygeometry(qbRenderModule module,
                                     uint32_t geometry_id) {
  WITH_MODULE(module, destroygeometry, geometry_id);
}

void qb_rendermodule_drawgeometry(qbRenderModule module,
                                  uint32_t geometry_id,
                                  size_t indices_count,
                                  size_t indices_offest) {
  WITH_MODULE(module, drawgeometry, geometry_id, indices_count, indices_offest);
}

uint32_t qb_rendermodule_loadprogram(qbRenderModule module, const char* vs_file, const char* fs_file) {
  return WITH_MODULE(module, loadprogram, vs_file, fs_file);
}

void qb_rendermodule_useprogram(qbRenderModule module, uint32_t program) {
  WITH_MODULE(module, useprogram, program);
}
