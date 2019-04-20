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

// Null-terminated linked list of user-defined extensions. Can be extended by
// defining a struct with the qbRenderExt_ as the first member. A pointer to
// the new struct is compatible with the qbRenderExt type.
typedef struct qbRenderExt_ {
  qbRenderExt_* next;
  const char* name;
} qbRenderExt_, *qbRenderExt;

typedef enum {
  QB_PIXEL_FORMAT_R8,
  QB_PIXEL_FORMAT_RG8,
  QB_PIXEL_FORMAT_RGB8,
  QB_PIXEL_FORMAT_RGBA8,
  QB_PIXEL_FORMAT_D32,
  QB_PIXEL_FORMAT_D24_S8,
  QB_PIXEL_FORMAT_S8
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
  uint32_t color_binding;

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
  const char* name;

  qbImageType type;
  bool generate_mipmaps;

  qbRenderExt ext;
} qbImageAttr_, *qbImageAttr;

// name = "qbPixelAlignmentOglExt_"
typedef struct {
  qbRenderExt_ ext;
  char alignment;
} qbPixelAlignmentOglExt_;

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
  const char* name;

  void* data;
  size_t size;
  size_t elem_size;

  qbGpuBufferType buffer_type;
  int sharing_type;

  qbRenderExt ext;
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
  const char* name;

  uint32_t binding;
  qbShaderResourceType resource_type;
  qbShaderStage stages;
} qbShaderResourceInfo_, *qbShaderResourceInfo;

typedef struct {
  const char* name;

  qbBufferBinding bindings;
  size_t bindings_count;

  qbVertexAttribute attributes;
  size_t attributes_count;

  size_t instance_count;

  qbRenderExt ext;
} qbMeshBufferAttr_, *qbMeshBufferAttr;

typedef struct {
  qbFrameBufferAttachment attachments;

  glm::vec4 color;
  float depth;
  uint32_t stencil;
} qbClearValue_;

typedef enum {
  QB_CULL_NONE,
  QB_CULL_FRONT,
  QB_CULL_BACK,
  QB_CULL_FRONT_BACK,
} qbCullFace;

typedef enum {
  QB_FRONT_FACE_CW,
  QB_FRONT_FACE_CCW,
} qbFrontFace;

typedef enum {
  QB_DEPTH_FUNC_NEVER,
  QB_DEPTH_FUNC_LESS,
  QB_DEPTH_FUNC_EQUAL,
  QB_DEPTH_FUNC_LEQUAL,
  QB_DEPTH_FUNC_GREATER,
  QB_DEPTH_FUNC_NOTEQUAL,
  QB_DEPTH_FUNC_GEQUAL,
  QB_DEPTH_FUNC_ALWAYS,
} qbDepthFunc;

typedef struct {
  bool depth_test_enable;
  bool depth_write_enable;
  qbDepthFunc depth_compare_op;

  bool stencil_test_enable;
} qbDepthStencilState;

typedef struct {
  const char* name;

  qbFrameBuffer frame_buffer;

  qbBufferBinding bindings;
  uint32_t bindings_count;

  qbVertexAttribute attributes;
  size_t attributes_count;

  qbShaderModule shader;

  qbClearValue_ clear;
  qbCullFace cull;

  glm::vec4 viewport;
  float viewport_scale;

  qbRenderExt ext;
} qbRenderPassAttr_;
typedef const qbRenderPassAttr_* qbRenderPassAttr;

typedef struct {
  const char* vs;
  const char* fs;
  const char* gs;

  qbShaderResourceInfo resources;
  size_t resources_count;
} qbShaderModuleAttr_, *qbShaderModuleAttr;

void qb_shadermodule_create(qbShaderModule* shader, qbShaderModuleAttr attr);
void qb_shadermodule_destroy(qbShaderModule* shader);
void qb_shadermodule_attachuniforms(qbShaderModule module, size_t count, uint32_t bindings[], qbGpuBuffer uniforms[]);
void qb_shadermodule_attachsamplers(qbShaderModule module, size_t count, uint32_t bindings[], qbImageSampler samplers[]);

qbPixelMap qb_pixelmap_create(uint32_t width, uint32_t height, qbPixelFormat format, void* pixels);
void qb_pixelmap_destroy(qbPixelMap* pixel_map);
qbResult qb_pixelmap_drawto(qbPixelMap src, qbPixelMap dest, glm::vec2 src_rect, glm::vec2 dest_rect);

void qb_renderpipeline_create(qbRenderPipeline* pipeline);
void qb_renderpipeline_destroy(qbRenderPipeline* pipeline);
void qb_renderpipeline_run(qbRenderPipeline render_pipeline, qbRenderEvent event);
void qb_renderpipeline_append(qbRenderPipeline pipeline, qbRenderPass pass);
void qb_renderpipeline_prepend(qbRenderPipeline pipeline, qbRenderPass pass);
size_t qb_renderpipeline_passes(qbRenderPipeline pipeline, qbRenderPass** passes);
void qb_renderpipeline_update(qbRenderPipeline pipeline, size_t count, qbRenderPass* pass);
void qb_renderpipeline_remove(qbRenderPipeline pipeline, qbRenderPass pass);

void qb_framebuffer_create(qbFrameBuffer* frame_buffer, qbFrameBufferAttr attr);
void qb_framebuffer_destroy(qbFrameBuffer* frame_buffer);
qbImage qb_framebuffer_rendertarget(qbFrameBuffer frame_buffer);
qbImage qb_framebuffer_depthtarget(qbFrameBuffer frame_buffer);
qbImage qb_framebuffer_stenciltarget(qbFrameBuffer frame_buffer);
qbImage qb_framebuffer_depthstenciltarget(qbFrameBuffer frame_buffer);

void qb_renderpass_create(qbRenderPass* render_pass, qbRenderPassAttr attr);
void qb_renderpass_destroy(qbRenderPass* render_pass);
qbFrameBuffer* qb_renderpass_frame(qbRenderPass render_pass);
const char* qb_renderpass_name(qbRenderPass render_pass);
qbRenderExt qb_renderpass_ext(qbRenderPass render_pass);
size_t qb_renderpass_bindings(qbRenderPass render_pass, qbBufferBinding* bindings);
size_t qb_renderpass_buffers(qbRenderPass render_pass, qbMeshBuffer** buffers);
size_t qb_renderpass_resources(qbRenderPass render_pass, qbShaderResourceInfo* resources);
size_t qb_renderpass_attributes(qbRenderPass render_pass, qbVertexAttribute* attributes);
size_t qb_renderpass_uniforms(qbRenderPass render_pass, uint32_t** bindings, qbGpuBuffer** uniforms);
size_t qb_renderpass_samplers(qbRenderPass render_pass, uint32_t** bindings, qbImageSampler** samplers);
void qb_renderpass_append(qbRenderPass render_pass, qbMeshBuffer buffer);
void qb_renderpass_prepend(qbRenderPass render_pass, qbMeshBuffer buffer);
void qb_renderpass_remove(qbRenderPass render_pass, qbMeshBuffer buffer);
void qb_renderpass_update(qbRenderPass render_pass, size_t count, qbMeshBuffer* buffers);

void qb_imagesampler_create(qbImageSampler* sampler, qbImageSamplerAttr attr);
void qb_imagesampler_destroy(qbImageSampler* sampler);

void qb_image_create(qbImage* image, qbImageAttr attr, qbPixelMap pixel_map);
void qb_image_raw(qbImage* image, qbImageAttr attr, qbPixelFormat format, uint32_t width, uint32_t height, void* pixels);
void qb_image_destroy(qbImage* image);
const char* qb_image_name(qbImage image);
void qb_image_load(qbImage* image, qbImageAttr attr, const char* file);
void qb_image_update(qbImage image, qbImageType type, qbPixelFormat format, glm::vec3 offset, glm::vec3 sizes);

void qb_gpubuffer_create(qbGpuBuffer* buffer, qbGpuBufferAttr attr);
void qb_gpubuffer_destroy(qbGpuBuffer* buffer);
const char* qb_gpubuffer_name(qbGpuBuffer buffer);
qbRenderExt qb_gpubuffer_ext(qbGpuBuffer buffer);
void qb_gpubuffer_update(qbGpuBuffer buffer, intptr_t offset, size_t size, void* data);
void qb_gpubuffer_copy(qbGpuBuffer dst, qbGpuBuffer src, intptr_t dst_offset, intptr_t src_offset, size_t size);

void qb_meshbuffer_create(qbMeshBuffer* buffer, qbMeshBufferAttr attr);
void qb_meshbuffer_destroy(qbMeshBuffer* buffer);
const char* qb_meshbuffer_name(qbMeshBuffer buffer);
qbRenderExt qb_meshbuffer_ext(qbMeshBuffer buffer);
void qb_meshbuffer_attachvertices(qbMeshBuffer buffer, qbGpuBuffer vertices[]);
void qb_meshbuffer_attachindices(qbMeshBuffer buffer, qbGpuBuffer indices);
void qb_meshbuffer_attachimages(qbMeshBuffer buffer, size_t count, uint32_t bindings[], qbImage images[]);
void qb_meshbuffer_attachuniforms(qbMeshBuffer buffer, size_t count, uint32_t bindings[], qbGpuBuffer uniforms[]);
size_t qb_meshbuffer_vertices(qbMeshBuffer buffer, qbGpuBuffer** vertices);
size_t qb_meshbuffer_images(qbMeshBuffer buffer, qbImage** images);
size_t qb_meshbuffer_uniforms(qbMeshBuffer buffer, qbGpuBuffer** uniforms);
void qb_meshbuffer_indices(qbMeshBuffer buffer, qbGpuBuffer* indices);

#endif  // CUBEZ_RENDER_MODULE__H