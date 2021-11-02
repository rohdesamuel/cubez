#ifndef RENDER_PIPELINE_DEFS__H
#define RENDER_PIPELINE_DEFS__H

#include <cubez/render_pipeline.h>
#include <cubez/common.h>

#define GL3_PROTOTYPES 1
#include <GL/glew.h>
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include "shader.h"

#include <mutex>
#include <string>
#include <vector>

#ifndef _DEBUG
#define CHECK_GL()  {GLenum err = glGetError(); if (err) FATAL(err) }
#else
#define CHECK_GL()
#endif   

constexpr std::vector<std::string> shader_extensions() {
  return { "GL_ARB_shading_language_420pack" };
}

typedef struct qbRenderPipeline_ {
  qbShaderModule shader_module;
  qbGeometryDescriptor geometry;
  qbRenderPass render_pass;
  qbColorBlendState blend_state;
  qbViewportState viewport_state;
  qbRasterizationInfo rasterization_info;
  qbShaderResourcePipelineLayout pipeline_layout;

  GLuint render_vao;

  qbRenderExt ext;
} qbRenderPipeline_;

typedef struct qbFrameBuffer_ {
  uint32_t id;
  std::vector<qbFramebufferAttachment_> attachments;
  std::vector<GLenum> draw_buffers;
  std::vector<qbImage> render_targets;

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
  uint32_t depth;
  qbPixelFormat format;
  uint8_t* pixels;

  size_t size;
  size_t pixel_size;
  size_t width_size;
  size_t height_size;
  size_t depth_size;

} qbPixelMap_;

typedef struct qbImage_ {
  const char* name;
  qbRenderExt ext;

  uint32_t id;
  qbImageType type;
  qbPixelFormat format;

  uint32_t width;
  uint32_t height;
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

  std::vector<uint32_t> sampler_bindings;
  std::vector<qbImage> images;

  qbGeometryDescriptor_ descriptor;

  size_t instance_count;
  size_t count;
} qbMeshBuffer_;

typedef struct qbShaderModule_ {
  ShaderProgram* shader;
} qbShaderModule_;

typedef struct qbRenderPass_ {
  const char* name;
  qbRenderExt ext;

  std::vector<qbFramebufferAttachmentRef_> attachments;
} qbRenderPass_;

typedef struct qbRenderGroup_ {
  std::vector<qbMeshBuffer> meshes;

  std::vector<uint32_t> uniform_bindings;
  std::vector<qbGpuBuffer> uniforms;

  std::vector<uint32_t> sampler_bindings;
  std::vector<qbImage> images;

  qbDrawMode mode;
} qbRenderGroup_, * qbRenderGroup;

typedef struct qbSurface_ {
  std::vector<qbFrameBuffer> targets;

  qbRenderPass pass;
  qbMeshBuffer dbo;
} qbSurface_, * qbSurface;

typedef struct qbSwapchain_ {
  std::vector<qbImage> images;
  uint32_t front_image;
  uint32_t back_image;

  std::vector<qbTask> tasks;
  qbTask front_task;
  qbTask back_task;

  qbRenderPipeline present;
  qbExtent_ extent;
  qbShaderModule shader_module;

  GLuint present_vao;
  GLuint present_vbo;

  qbImageSampler swapchain_imager_sampler;

  std::mutex image_swap_mu;
} qbSwapchain_, * qbSwapchain;

typedef struct qbShaderResourceLayout_ {
  std::vector<qbShaderResourceBinding_> bindings;

} qbShaderResourceLayout_, * qbShaderResourceLayout;

typedef struct qbShaderResourcePipelineLayout_ {
  std::vector<qbShaderResourceLayout> layouts;

} qbShaderResourcePipelineLayout_, * qbShaderResourcePipelineLayout;

typedef struct qbShaderResourceSet_ {
  std::vector<qbShaderResourceBinding> bindings;
  std::vector<qbGpuBuffer> uniforms;
  std::vector<qbImage> images;
  std::vector<qbImageSampler> samplers;
} qbShaderResourceSet_, * qbShaderResourceSet;

#endif  // RENDER_PIPELINE_DEFS__H