#ifndef CUBEZ_RENDER_MODULE__H
#define CUBEZ_RENDER_MODULE__H

#include <cubez/cubez.h>
#include <glm/glm.hpp>

typedef struct qbRenderPipeline_* qbRenderPipeline;
typedef struct qbRenderModule_* qbRenderModule;
typedef struct qbRenderPass_* qbRenderPass;
typedef struct qbVertexAttribute_* qbVertexAttribute;
typedef struct qbShaderModule_* qbShaderModule;
typedef struct qbFrameBuffer_* qbFrameBuffer;
typedef struct qbImageSampler_* qbImageSampler;
typedef struct qbPixelMap_* qbPixelMap;
typedef struct qbImage_* qbImage;
typedef struct qbGpuBuffer_* qbGpuBuffer;
typedef struct qbMeshBuffer_* qbMeshBuffer;
typedef struct qbRenderEvent_* qbRenderEvent;

typedef enum {
  QB_PIXEL_FORMAT_RGB8,
  QB_PIXEL_FORMAT_RGBA8
} qbPixelFormat;

typedef enum {
  QB_GPU_BUFFER_TYPE_VERTEX,
  QB_GPU_BUFFER_TYPE_INDEX,
  QB_GPU_BUFFER_TYPE_UNIFORM,
} qbGpuBufferType;

typedef enum {
  QB_VERTEX_INPUT_RATE_VERTEX,
  QB_VERTEX_INPUT_RATE_INSTANCE,
} qbVertexInputRate;

typedef enum {
  QB_COLOR_ATTACHMENT = 0x0001,
  QB_DEPTH_ATTACHMENT = 0x0002,
  QB_STENCIL_ATTACHMENT = 0x0004,
  QB_DEPTH_STENCIL_ATTACHMENT = 0x0008,
} qbFrameBufferAttachment;

typedef struct {
  uint32_t width;
  uint32_t height;

  qbFrameBufferAttachment attachments;

} qbFrameBufferAttr_;
typedef const qbFrameBufferAttr_* qbFrameBufferAttr;

typedef enum {
  QB_IMAGE_TYPE_1D,
  QB_IMAGE_TYPE_2D,
  QB_IMAGE_TYPE_3D,
  QB_IMAGE_TYPE_CUBE,
  QB_IMAGE_TYPE_1D_ARRAY,
  QB_IMAGE_TYPE_2D_ARRAY,
  QB_IMAGE_TYPE_CUBE_ARRAY,
} qbImageType;

typedef struct {
  qbImageType type;
  qbPixelMap pixel_map;

  bool generate_mipmaps;
} qbImageAttr_, *qbImageAttr;

typedef enum {
  QB_FILTER_TYPE_NEAREST,
  QB_FILTER_TYPE_LINEAR,
  QB_FILTER_TYPE_NEAREST_MIPMAP_NEAREST,
  QB_FILTER_TYPE_LINEAR_MIPMAP_NEAREST,
  QB_FILTER_TYPE_NEAREST_MIPMAP_LINEAR,
  QB_FILTER_TYPE_LIENAR_MIPMAP_LINEAR,
} qbFilterType;

typedef enum {
  QB_IMAGE_WRAP_TYPE_REPEAT,
  QB_IMAGE_WRAP_TYPE_MIRRORED_REPEAT,
  QB_IMAGE_WRAP_TYPE_CLAMP_TO_EDGE,
  QB_IMAGE_WRAP_TYPE_CLAMP_TO_BORDER,
} qbImageWrapType;

typedef struct {
  qbImageType image_type;

  qbFilterType mag_filter;
  qbFilterType min_filter;

  qbImageWrapType s_wrap;
  qbImageWrapType t_wrap;
  qbImageWrapType r_wrap;

} qbImageSamplerAttr_, *qbImageSamplerAttr;

typedef struct {
  void* data;
  size_t size;
  size_t elem_size;

  qbGpuBufferType buffer_type;
  int sharing_type;
} qbGpuBufferAttr_, *qbGpuBufferAttr;

typedef struct {
  uint32_t binding;
  uint32_t stride;
  qbVertexInputRate input_rate;
  

} qbBufferBinding_, *qbBufferBinding;

typedef enum {
  QB_VERTEX_ATTRIB_TYPE_BYTE,
  QB_VERTEX_ATTRIB_TYPE_UNSIGNED_BYTE,
  QB_VERTEX_ATTRIB_TYPE_SHORT,
  QB_VERTEX_ATTRIB_TYPE_UNSIGNED_SHORT,
  QB_VERTEX_ATTRIB_TYPE_INT,
  QB_VERTEX_ATTRIB_TYPE_UNSIGNED_INT,
  QB_VERTEX_ATTRIB_TYPE_HALF_FLOAT,
  QB_VERTEX_ATTRIB_TYPE_FLOAT,
  QB_VERTEX_ATTRIB_TYPE_DOUBLE,
  QB_VERTEX_ATTRIB_TYPE_FIXED,
} qbVertexAttribType;

typedef struct qbVertexAttribute_ {
  uint32_t binding;
  uint32_t location;

  size_t count;
  qbVertexAttribType type;
  bool normalized;
  const void* offset;
} qbVertexAttribute_;

typedef enum {
  QB_SHADER_STAGE_VERTEX = 0x0001,
  QB_SHADER_STAGE_FRAGMENT = 0x0002,
  QB_SHADER_STAGE_GEOMETRY = 0x0004,
} qbShaderStage;

typedef enum {
  QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER,
  QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER
} qbShaderResourceType;

typedef struct {
  uint32_t binding;
  qbShaderResourceType resource_type;
  qbShaderStage stages;
  const char* name;
} qbShaderResourceInfo_, *qbShaderResourceInfo;

typedef struct {
  qbBufferBinding bindings;
  size_t bindings_count;

  qbVertexAttribute attributes;
  size_t attributes_count;

  size_t instance_count;

} qbMeshBufferAttr_, *qbMeshBufferAttr;

typedef struct {
  qbFrameBuffer frame_buffer;

  qbBufferBinding bindings;
  uint32_t bindings_count;

  qbVertexAttribute attributes;
  size_t attributes_count;

  qbShaderModule shader_program;

  glm::vec4 viewport;
  float viewport_scale;

} qbRenderPassAttr_;
typedef const qbRenderPassAttr_* qbRenderPassAttr;

typedef struct {
  const char* vs;
  const char* fs;

  qbShaderResourceInfo resources;
  size_t resources_count;

} qbShaderModuleAttr_, *qbShaderModuleAttr;

void qb_shadermodule_create(qbShaderModule* shader, qbShaderModuleAttr attr);
void qb_shadermodule_attachuniforms(qbShaderModule module, size_t count, uint32_t bindings[], qbGpuBuffer uniforms[]);
void qb_shadermodule_attachsamplers(qbShaderModule module, size_t count, uint32_t bindings[], qbImageSampler samplers[]);

qbPixelMap qb_pixelmap_create(uint32_t width, uint32_t height, qbPixelFormat format, void* pixels);
qbResult qb_pixelmap_drawto(qbPixelMap src, qbPixelMap dest, glm::vec2 src_rect, glm::vec2 dest_rect);

void qb_renderpipeline_create(qbRenderPipeline* pipeline);
void qb_renderpipeline_appendrenderpass(qbRenderPipeline pipeline, qbRenderPass pass);
void qb_renderpipeline_run(qbRenderPipeline render_pipeline, qbRenderEvent event);

void qb_framebuffer_create(qbFrameBuffer* frame_buffer, qbFrameBufferAttr attr);
qbImage qb_framebuffer_rendertarget(qbFrameBuffer frame_buffer);

void qb_renderpass_create(qbRenderPass* render_pass, qbRenderPassAttr attr);
size_t qb_renderpass_bindings(qbRenderPass render_pass, qbBufferBinding* bindings);
size_t qb_renderpass_buffers(qbRenderPass render_pass, qbMeshBuffer** buffers);
size_t qb_renderpass_resources(qbRenderPass render_pass, qbShaderResourceInfo* resources);
size_t qb_renderpass_attributes(qbRenderPass render_pass, qbVertexAttribute* attributes);
size_t qb_renderpass_samplers(qbRenderPass render_pass, qbImageSampler** samplers);

void qb_renderpass_appendmeshbuffer(qbRenderPass render_pass, qbMeshBuffer buffer);
void qb_renderpass_removemeshbuffer(qbRenderPass render_pass, qbMeshBuffer buffer);
void qb_renderpass_updatemeshbuffers(qbRenderPass render_pass, size_t count, qbMeshBuffer* buffers);

void qb_imagesampler_create(qbImageSampler* sampler, qbImageSamplerAttr attr);
void qb_image_create(qbImage* image, qbImageAttr attr);
void qb_image_load(qbImage* image, qbImageAttr attr, const char* file);
void qb_image_sub(qbImage* dst, qbImage src, glm::vec2 offset, glm::vec2 sizes);
void qb_image_update(qbImage image, qbImageType type, qbPixelFormat format, glm::vec3 offset, glm::vec3 sizes);

void qb_gpubuffer_create(qbGpuBuffer* buffer, qbGpuBufferAttr attr);
void qb_gpubuffer_update(qbGpuBuffer buffer, intptr_t offset, size_t size, void* data);
void qb_gpubuffer_copy(qbGpuBuffer dst, qbGpuBuffer src, intptr_t dst_offset, intptr_t src_offset, size_t size);

void qb_meshbuffer_create(qbMeshBuffer* buffer, qbMeshBufferAttr attr);

void qb_meshbuffer_attachvertices(qbMeshBuffer buffer, qbGpuBuffer vertices[]);
void qb_meshbuffer_attachindices(qbMeshBuffer buffer, qbGpuBuffer indices);
void qb_meshbuffer_attachimages(qbMeshBuffer buffer, size_t count, uint32_t bindings[], qbImage images[]);
void qb_meshbuffer_attachuniforms(qbMeshBuffer buffer, size_t count, uint32_t bindings[], qbGpuBuffer uniforms[]);

size_t qb_meshbuffer_vertices(qbMeshBuffer buffer, qbGpuBuffer** vertices);
size_t qb_meshbuffer_images(qbMeshBuffer buffer, qbImage** images);
size_t qb_meshbuffer_uniforms(qbMeshBuffer buffer, qbGpuBuffer** uniforms);
void qb_meshbuffer_indices(qbMeshBuffer buffer, qbGpuBuffer* indices);

#endif  // CUBEZ_RENDER_MODULE__H