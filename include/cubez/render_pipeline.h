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

#ifndef CUBEZ_RENDER_PIPELINE__H
#define CUBEZ_RENDER_PIPELINE__H

#include <cubez/cubez.h>
#include <cglm/cglm.h>
#include <cglm/types-struct.h>

typedef struct qbRenderPipeline_* qbRenderPipeline;
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
typedef struct qbRenderGroup_* qbRenderGroup;
typedef struct qbSurface_* qbSurface;

// Null-terminated linked list of user-defined extensions. Can be extended by
// defining a struct with the qbRenderExt_ as the first member. A pointer to
// the new struct is compatible with the qbRenderExt type.
typedef struct qbRenderExt_ {
  struct qbRenderExt_* next;
  const char* name;
  void* ext;
} qbRenderExt_, *qbRenderExt;

typedef enum {
  QB_PIXEL_FORMAT_R8,
  QB_PIXEL_FORMAT_RG8,
  QB_PIXEL_FORMAT_RGB8,
  QB_PIXEL_FORMAT_RGBA8,
  QB_PIXEL_FORMAT_R16F,
  QB_PIXEL_FORMAT_RG16F,
  QB_PIXEL_FORMAT_RGB16F,
  QB_PIXEL_FORMAT_RGBA16F,
  QB_PIXEL_FORMAT_R32F,
  QB_PIXEL_FORMAT_RG32F,
  QB_PIXEL_FORMAT_RGB32F,
  QB_PIXEL_FORMAT_RGBA32F,
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
  qbFrameBufferAttachment attachments;

  vec4s color;
  float depth;
  uint32_t stencil;
} qbClearValue_, *qbClearValue;

typedef struct {
  uint32_t width;
  uint32_t height;

  qbFrameBufferAttachment* attachments;
  uint32_t* color_binding;
  size_t attachments_count;

  qbClearValue_ clear_value;
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
typedef struct qbPixelAlignmentOglExt_ {
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

typedef enum {
  QB_VERTEX_ATTRIB_NAME_VERTEX,
  QB_VERTEX_ATTRIB_NAME_NORMAL,
  QB_VERTEX_ATTRIB_NAME_COLOR,
  QB_VERTEX_ATTRIB_NAME_TEXTURE,
  QB_VERTEX_ATTRIB_NAME_INDEX,
  QB_VERTEX_ATTRIB_NAME_PARAM,
  QB_VERTEX_ATTRIB_NAME_UNUSED,
} qbVertexAttribName;

typedef struct qbVertexAttribute_ {
  uint32_t binding;
  uint32_t location;

  size_t count;
  qbVertexAttribType type;
  qbVertexAttribName name;
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
  qbBufferBinding bindings;
  size_t bindings_count;

  qbVertexAttribute attributes;
  size_t attributes_count;
} qbGeometryDescriptor_, *qbGeometryDescriptor;

typedef struct {
  const char* name;

  qbGeometryDescriptor_ descriptor;

  size_t instance_count;

  qbRenderExt ext;
} qbMeshBufferAttr_, *qbMeshBufferAttr;

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

  qbGeometryDescriptor_ supported_geometry;

  qbShaderModule shader;

  qbClearValue_ clear;
  qbCullFace cull;

  vec4s viewport;
  float viewport_scale;

  qbRenderExt ext;
} qbRenderPassAttr_;
typedef const qbRenderPassAttr_* qbRenderPassAttr;

typedef struct {
  const char* name;

  vec4s viewport;
  float viewport_scale;

  qbRenderExt ext;
} qbRenderPipelineAttr_, *qbRenderPipelineAttr;

typedef struct {
  const char* vs;
  const char* fs;
  const char* gs;

  qbShaderResourceInfo resources;
  size_t resources_count;
} qbShaderModuleAttr_, *qbShaderModuleAttr;

typedef struct qbSurfaceAttr_ {
  uint32_t width;
  uint32_t height;

  const char* vs;
  const char* fs;

  qbShaderResourceInfo resources;
  size_t resources_count;

  qbImageSampler* samplers;
  uint32_t* sampler_bindings;
  size_t samplers_count;

  qbGpuBuffer* uniforms;
  uint32_t* uniform_bindings;
  size_t uniforms_count;

  qbPixelFormat pixel_format;
  size_t target_count;
} qbSurfaceAttr_, *qbSurfaceAttr;


// TODO: Should this be recursive to allow for hierarchical models?
// TODO: Should there be a transform (position+orientation) for Render Groups?
typedef struct qbRenderGroupAttr_ {
  qbMeshBuffer* meshes;
  size_t mesh_count;
} qbRenderGroupAttr_, *qbRenderGroupAttr;

QB_API void qb_shadermodule_create(qbShaderModule* shader, qbShaderModuleAttr attr);
QB_API void qb_shadermodule_destroy(qbShaderModule* shader);
QB_API void qb_shadermodule_attachuniforms(qbShaderModule module, size_t count,
                                           uint32_t bindings[], qbGpuBuffer uniforms[]);

// Attaches the samplers to the given module to inform the graphics driver how
// to sample a given texture. The given samplers must have a corresponding
// uniform already attached. If any given sampler does not have a name, it
// will be taken from the associated uniform name.
QB_API void qb_shadermodule_attachsamplers(qbShaderModule module, size_t count,
                                    uint32_t bindings[], qbImageSampler samplers[]);

QB_API qbPixelMap qb_pixelmap_create(uint32_t width, uint32_t height, uint32_t depth, qbPixelFormat format, void* pixels);
QB_API void qb_pixelmap_destroy(qbPixelMap* pixel_map);
QB_API qbResult qb_pixelmap_drawto(qbPixelMap src, qbPixelMap dest, vec2s src_rect, vec2s dest_rect);

QB_API void qb_renderpipeline_create(qbRenderPipeline* pipeline, qbRenderPipelineAttr attr);
QB_API void qb_renderpipeline_destroy(qbRenderPipeline* pipeline);
QB_API void qb_renderpipeline_present(qbRenderPipeline render_pipeline, qbFrameBuffer frame_buffer,
                                      qbRenderEvent event);
QB_API void qb_renderpipeline_resize(qbRenderPipeline render_pipeline, vec4s viewport);
QB_API void qb_renderpipeline_append(qbRenderPipeline pipeline, qbRenderPass pass);
QB_API void qb_renderpipeline_prepend(qbRenderPipeline pipeline, qbRenderPass pass);
QB_API size_t qb_renderpipeline_passes(qbRenderPipeline pipeline, qbRenderPass** passes);
QB_API void qb_renderpipeline_update(qbRenderPipeline pipeline, size_t count, qbRenderPass* pass);
QB_API size_t qb_renderpipeline_remove(qbRenderPipeline pipeline, qbRenderPass pass);

QB_API void qb_framebuffer_create(qbFrameBuffer* frame_buffer, qbFrameBufferAttr attr);
QB_API void qb_framebuffer_destroy(qbFrameBuffer* frame_buffer);
QB_API void qb_framebuffer_clear(qbFrameBuffer frame_buffer, qbClearValue clear_value);
QB_API qbImage qb_framebuffer_target(qbFrameBuffer frame_buffer, size_t i);
QB_API size_t qb_framebuffer_rendertargets(qbFrameBuffer frame_buffer, qbImage** targets);
QB_API qbImage qb_framebuffer_depthtarget(qbFrameBuffer frame_buffer);
QB_API qbImage qb_framebuffer_stenciltarget(qbFrameBuffer frame_buffer);
QB_API qbImage qb_framebuffer_depthstenciltarget(qbFrameBuffer frame_buffer);
QB_API void qb_framebuffer_resize(qbFrameBuffer frame_buffer, uint32_t width, uint32_t height);
QB_API uint32_t qb_framebuffer_width(qbFrameBuffer frame_buffer);
QB_API uint32_t qb_framebuffer_height(qbFrameBuffer frame_buffer);

QB_API void qb_renderpass_create(qbRenderPass* render_pass, qbRenderPassAttr attr);
QB_API void qb_renderpass_destroy(qbRenderPass* render_pass);
QB_API void qb_renderpass_draw(qbRenderPass render_pass, qbFrameBuffer frame_buffer);
QB_API void qb_renderpass_drawto(qbRenderPass render_pass, qbFrameBuffer frame_buffer, size_t count, qbRenderGroup* groups);
QB_API void qb_renderpass_resize(qbRenderPass render_pass, vec4s viewport);
QB_API const char* qb_renderpass_name(qbRenderPass render_pass);
QB_API qbRenderExt qb_renderpass_ext(qbRenderPass render_pass);

QB_API qbGeometryDescriptor qb_renderpass_supportedgeometry(qbRenderPass render_pass);
QB_API size_t qb_renderpass_bindings(qbRenderPass render_pass, qbBufferBinding* bindings);
QB_API size_t qb_renderpass_groups(qbRenderPass render_pass, qbRenderGroup** groups);
QB_API size_t qb_renderpass_resources(qbRenderPass render_pass, qbShaderResourceInfo* resources);
QB_API size_t qb_renderpass_attributes(qbRenderPass render_pass, qbVertexAttribute* attributes);
QB_API size_t qb_renderpass_uniforms(qbRenderPass render_pass, uint32_t** bindings, qbGpuBuffer** uniforms);
QB_API size_t qb_renderpass_samplers(qbRenderPass render_pass, uint32_t** bindings, qbImageSampler** samplers);
QB_API void qb_renderpass_append(qbRenderPass render_pass, qbRenderGroup group);
QB_API void qb_renderpass_prepend(qbRenderPass render_pass, qbRenderGroup group);
QB_API size_t qb_renderpass_remove(qbRenderPass render_pass, qbRenderGroup group);
QB_API void qb_renderpass_update(qbRenderPass render_pass, size_t count, qbRenderGroup* groups);

QB_API void qb_imagesampler_create(qbImageSampler* sampler, qbImageSamplerAttr attr);
QB_API void qb_imagesampler_destroy(qbImageSampler* sampler);
QB_API const char* qb_imagesampler_name(qbImageSampler sampler);

QB_API void qb_image_create(qbImage* image, qbImageAttr attr, qbPixelMap pixel_map);
QB_API void qb_image_raw(qbImage* image, qbImageAttr attr, qbPixelFormat format, uint32_t width, uint32_t height, void* pixels);
QB_API void qb_image_destroy(qbImage* image);
QB_API const char* qb_image_name(qbImage image);
QB_API void qb_image_load(qbImage* image, qbImageAttr attr, const char* file);
QB_API void qb_image_update(qbImage image, ivec3s offset, ivec3s sizes, void* data);

QB_API void qb_gpubuffer_create(qbGpuBuffer* buffer, qbGpuBufferAttr attr);
QB_API void qb_gpubuffer_destroy(qbGpuBuffer* buffer);
QB_API const char* qb_gpubuffer_name(qbGpuBuffer buffer);
QB_API qbRenderExt qb_gpubuffer_ext(qbGpuBuffer buffer);
QB_API void qb_gpubuffer_update(qbGpuBuffer buffer, intptr_t offset, size_t size, void* data);
QB_API void qb_gpubuffer_copy(qbGpuBuffer dst, qbGpuBuffer src, intptr_t dst_offset, intptr_t src_offset, size_t size);
QB_API void qb_gpubuffer_swap(qbGpuBuffer a, qbGpuBuffer b);

QB_API void qb_meshbuffer_create(qbMeshBuffer* buffer, qbMeshBufferAttr attr);
QB_API void qb_meshbuffer_destroy(qbMeshBuffer* buffer);
QB_API const char* qb_meshbuffer_name(qbMeshBuffer buffer);
QB_API qbRenderExt qb_meshbuffer_ext(qbMeshBuffer buffer);
QB_API void qb_meshbuffer_attachvertices(qbMeshBuffer buffer, qbGpuBuffer vertices[]);
QB_API void qb_meshbuffer_attachindices(qbMeshBuffer buffer, qbGpuBuffer indices);
QB_API void qb_meshbuffer_attachimages(qbMeshBuffer buffer, size_t count, uint32_t bindings[], qbImage images[]);
QB_API void qb_meshbuffer_attachuniforms(qbMeshBuffer buffer, size_t count, uint32_t bindings[], qbGpuBuffer uniforms[]);
QB_API size_t qb_meshbuffer_vertices(qbMeshBuffer buffer, qbGpuBuffer** vertices);
QB_API size_t qb_meshbuffer_images(qbMeshBuffer buffer, qbImage** images);
QB_API size_t qb_meshbuffer_uniforms(qbMeshBuffer buffer, qbGpuBuffer** uniforms);
QB_API void qb_meshbuffer_indices(qbMeshBuffer buffer, qbGpuBuffer* indices);

QB_API void qb_rendergroup_create(qbRenderGroup* group, qbRenderGroupAttr attr);
QB_API void qb_rendergroup_destroy(qbRenderGroup* group);
QB_API void qb_rendergroup_attachimages(qbRenderGroup buffer, size_t count, uint32_t bindings[], qbImage images[]);
QB_API void qb_rendergroup_attachuniforms(qbRenderGroup buffer, size_t count, uint32_t bindings[], qbGpuBuffer uniforms[]);
QB_API void qb_rendergroup_addimage(qbRenderGroup buffer, qbImage image, uint32_t binding);
QB_API void qb_rendergroup_adduniform(qbRenderGroup buffer, qbGpuBuffer uniform, uint32_t binding);
QB_API qbImage qb_rendergroup_findimage(qbRenderGroup buffer, qbImage image);
QB_API qbGpuBuffer qb_rendergroup_finduniform(qbRenderGroup buffer, qbGpuBuffer uniform);
QB_API qbGpuBuffer qb_rendergroup_finduniform_byname(qbRenderGroup buffer, const char* name);
QB_API qbGpuBuffer qb_rendergroup_finduniform_bybinding(qbRenderGroup buffer, uint32_t binding);

QB_API size_t qb_rendergroup_removeimage(qbRenderGroup buffer, qbImage image);
QB_API size_t qb_rendergroup_removeuniform(qbRenderGroup buffer, qbGpuBuffer uniform);
QB_API size_t qb_rendergroup_removeuniform_byname(qbRenderGroup buffer, const char* name);
QB_API size_t qb_rendergroup_removeuniform_bybinding(qbRenderGroup buffer, uint32_t binding);

QB_API size_t qb_rendergroup_meshes(qbRenderGroup group, qbMeshBuffer** buffers);
QB_API void qb_rendergroup_append(qbRenderGroup group, qbMeshBuffer buffer);
QB_API void qb_rendergroup_prepend(qbRenderGroup group, qbMeshBuffer buffer);
QB_API size_t qb_rendergroup_remove(qbRenderGroup group, qbMeshBuffer buffer);
QB_API void qb_rendergroup_update(qbRenderGroup group, size_t count, qbMeshBuffer* buffers);
QB_API size_t qb_rendergroup_images(qbRenderGroup buffer, qbImage** images);
QB_API size_t qb_rendergroup_uniforms(qbRenderGroup buffer, qbGpuBuffer** uniforms);

QB_API qbRenderExt qb_renderext_find(qbRenderExt extensions, const char* ext_name);
QB_API void qb_renderext_add(qbRenderExt* extensions, const char* ext_name, void* data);

// Frees the extensions linked list without freeing the underlying user data.
// It is assumed that the extension data is statically defined.
QB_API void qb_renderext_destroy(qbRenderExt* extensions);

QB_API void qb_surface_create(qbSurface* surface, qbSurfaceAttr attr);
QB_API void qb_surface_destroy(qbSurface* surface);
QB_API void qb_surface_resize(qbSurface surface, uint32_t width, uint32_t height);
QB_API void qb_surface_draw(qbSurface surface, qbImage* input, qbFrameBuffer output);
QB_API qbFrameBuffer qb_surface_target(qbSurface surface, size_t i);
QB_API qbImage qb_surface_image(qbSurface surface, size_t i);

#endif  // CUBEZ_RENDER_PIPELINE__H