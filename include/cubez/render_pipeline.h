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
#include <cubez/async.h>
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
typedef struct qbImageView_* qbImageView;
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

#define QB_COLOR_ASPECT_BIT 0x0001
#define QB_DEPTH_ASPECT_BIT 0x0002
#define QB_STENCIL_ASPECT_BIT 0x0004
#define QB_DEPTH_STENCIL_ASPECT_BIT 0x0008

typedef enum {
  QB_COLOR_ASPECT,
  QB_DEPTH_ASPECT,
  QB_STENCIL_ASPECT,
  QB_DEPTH_STENCIL_ASPECT
} qbImageAspect;

typedef struct {
  union {
    float float32[4];
    int32_t int32[4];
    uint32_t uint32[4];
  } color;

  float depth;
  uint32_t stencil;
} qbClearValue_, *qbClearValue;

typedef struct qbFramebufferAttachment_ {
  qbImage image;
  qbImageAspect aspect;
} qbFramebufferAttachment_, *qbFramebufferAttachment;

#define QB_UNUSED_FRAMEBUFFER_ATTACHMENT (~0U)

typedef struct qbFramebufferAttachmentRef_ {
  // Index into the `attachments` array when creating the qbFrameBuffer.
  uint32_t attachment;
  qbImageAspect aspect;
} qbFramebufferAttachmentRef_, *qbFramebufferAttachmentRef;

typedef struct {
  qbRenderPass render_pass;

  qbFramebufferAttachment_* attachments;
  size_t attachments_count;

  uint32_t width, height;
  uint32_t layers;
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
  qbBool generate_mipmaps;

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

  // Unused.
  int sharing_type;

  qbRenderExt ext;
} qbGpuBufferAttr_, *qbGpuBufferAttr;

// A buffer binding is used to bind a qbGpuBuffer to a qbVertexAttribute.
typedef struct {
  // User given unique id for this given buffer. Unique within a given
  // qbGeometryDescripter_.
  uint32_t binding;

  // The distance in bytes between two consecutive elements within the buffer.
  uint32_t stride;

  // Specifying whether the vertex attribute is a function of the vertex index
  // or the instance vertex.
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
  QB_VERTEX_ATTRIB_NAME_POSITION,
  QB_VERTEX_ATTRIB_NAME_NORMAL,
  QB_VERTEX_ATTRIB_NAME_COLOR,
  QB_VERTEX_ATTRIB_NAME_TEXTURE,
  QB_VERTEX_ATTRIB_NAME_INDEX,
  QB_VERTEX_ATTRIB_NAME_PARAM,
  QB_VERTEX_ATTRIB_NAME_UNUSED,
} qbVertexAttribName;

typedef struct qbVertexAttribute_ {
  // The matching binding id of the qbBufferBinding_ that holds the data for
  // this attribute.
  uint32_t binding;

  // The shader binding location for this attribute.
  uint32_t location;

  // The number of components in this attribute.
  size_t count;
  qbVertexAttribType type;
  qbVertexAttribName name;
  qbBool normalized;
  const void* offset;
} qbVertexAttribute_;

#define QB_SHADER_STAGE_VERTEX 0x0001
#define QB_SHADER_STAGE_FRAGMENT 0x0002
#define QB_SHADER_STAGE_GEOMETRY 0x0004
typedef int32_t qbShaderStage;

typedef enum {
  QB_SHADER_RESOURCE_TYPE_IMAGE_SAMPLER,
  QB_SHADER_RESOURCE_TYPE_UNIFORM_BUFFER
} qbShaderResourceType;

typedef struct {
  const char* name;

  uint32_t binding;
  qbShaderResourceType resource_type;
  uint32_t resource_count;

  qbShaderStage stages;
} qbShaderResourceBinding_, *qbShaderResourceBinding;

typedef enum {
  QB_DRAW_MODE_POINT_LIST,
  QB_DRAW_MODE_LINE_LIST,
  QB_DRAW_MODE_LINE_STRIP,
  QB_DRAW_MODE_TRIANGLE_LIST,
  QB_DRAW_MODE_TRIANGLE_STRIP,
  QB_DRAW_MODE_TRIANGLE_FAN,

  // Short-hand.
  QB_DRAW_MODE_POINTS = QB_DRAW_MODE_POINT_LIST,
  QB_DRAW_MODE_LINES = QB_DRAW_MODE_LINE_LIST,
  QB_DRAW_MODE_TRIANGLES = QB_DRAW_MODE_TRIANGLE_LIST,
} qbDrawMode;

typedef enum {
  QB_POLYGON_MODE_POINT,
  QB_POLYGON_MODE_LINE,
  QB_POLYGON_MODE_FILL,
} qbPolygonMode;

typedef enum {
  QB_FRONT_FACE_CW,
  QB_FRONT_FACE_CCW,
} qbFrontFace;

typedef enum {
  QB_FACE_NONE,
  QB_FACE_FRONT,
  QB_FACE_BACK,
  QB_FACE_FRONT_AND_BACK,
} qbFace;

typedef enum {
  QB_RENDER_TEST_FUNC_NEVER,
  QB_RENDER_TEST_FUNC_LESS,
  QB_RENDER_TEST_FUNC_EQUAL,
  QB_RENDER_TEST_FUNC_LEQUAL,
  QB_RENDER_TEST_FUNC_GREATER,
  QB_RENDER_TEST_FUNC_NOTEQUAL,
  QB_RENDER_TEST_FUNC_GEQUAL,
  QB_RENDER_TEST_FUNC_ALWAYS,
} qbRenderTestFunc;

typedef struct qbDepthStencilState_ {
  qbBool depth_test_enable;
  qbBool depth_write_enable;
  qbRenderTestFunc depth_compare_op;

  qbBool stencil_test_enable;
  qbRenderTestFunc stencil_compare_op;
  uint32_t stencil_ref_value;
  uint32_t stencil_mask;

} qbDepthStencilState_, * qbDepthStencilState;

typedef struct qbRasterizationInfo_ {
  qbPolygonMode raster_mode;
  qbFace raster_face;
  qbFrontFace front_face;
  qbFace cull_face;  
  qbBool enable_depth_clamp;

  float point_size;
  float line_width;

  qbDepthStencilState depth_stencil_state;

} qbRasterizationInfo_, *qbRasterizationInfo;

typedef struct {
  qbBufferBinding bindings;
  size_t bindings_count;

  qbVertexAttribute attributes;
  size_t attributes_count;

  qbDrawMode mode;
} qbGeometryDescriptor_, *qbGeometryDescriptor;

typedef struct {
  const char* name;

  qbGeometryDescriptor_ descriptor;

  size_t instance_count;

  qbRenderExt ext;
} qbMeshBufferAttr_, *qbMeshBufferAttr;

typedef struct qbRenderPassAttr_ {
  const char* name;

  qbFramebufferAttachmentRef_* attachments;
  uint32_t attachments_count;

  qbRenderExt ext;
} qbRenderPassAttr_, *qbRenderPassAttr;

typedef enum {
  QB_BLEND_FACTOR_ZERO,
  QB_BLEND_FACTOR_ONE,
  QB_BLEND_FACTOR_SRC_COLOR,
  QB_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
  QB_BLEND_FACTOR_DST_COLOR,
  QB_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
  QB_BLEND_FACTOR_SRC_ALPHA,
  QB_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
  QB_BLEND_FACTOR_DST_ALPHA,
  QB_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
  QB_BLEND_FACTOR_CONSTANT_COLOR,
  QB_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,
  QB_BLEND_FACTOR_CONSTANT_ALPHA,
  QB_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA,
  QB_BLEND_FACTOR_SRC_ALPHA_SATURATE,
} qbBlendFactor;

typedef enum {
  QB_BLEND_EQUATION_ADD,
  QB_BLEND_EQUATION_SUBTRACT,
  QB_BLEND_EQUATION_REVERSE_SUBTRACT,
  QB_BLEND_EQUATION_MIN,
  QB_BLEND_EQUATION_MAX,
} qbBlendEquation;

typedef struct qbColorBlend_ {
  qbBlendEquation op;
  qbBlendFactor src;
  qbBlendFactor dst;
} qbColorBlend_, *qbColorBlend;

typedef struct qbColorBlendState_ {
  qbBool blend_enable;

  qbColorBlend_ rgb_blend;
  qbColorBlend_ alpha_blend;

  // Accessed when blend factor is:
  //  - QB_BLEND_FACTOR_CONSTANT_COLOR
  //  - QB_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR
  //  - QB_BLEND_FACTOR_CONSTANT_ALPHA
  //  - QB_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA
  vec4s blend_color;

} qbColorBlendState_, *qbColorBlendState;

typedef struct qbViewport_ {
  float x, y;
  float w, h;
  float min_depth, max_depth;
} qbViewport_, *qbViewport;

typedef struct qbRect_ {
  float x, y;
  float w, h;
} qbRect_, *qbRect;

typedef struct qbExtent_ {
  float w, h;
} qbExtent_, *qbExtent;

typedef struct qbViewportState_ {
  qbViewport_ viewport;
  qbRect_ scissor;
} qbViewportState_, *qbViewportState;

typedef struct qbShaderResourceLayoutAttr_ {
  uint32_t binding_count;
  qbShaderResourceBinding bindings;
} qbShaderResourceLayoutAttr_, *qbShaderResourceLayoutAttr;

typedef struct qbShaderResourceLayout_* qbShaderResourceLayout;
QB_API void qb_shaderresourcelayout_create(qbShaderResourceLayout* resource_layout, qbShaderResourceLayoutAttr attr);

typedef struct qbShaderResourcePipelineLayoutAttr_ {
  uint32_t layout_count;
  qbShaderResourceLayout* layouts;
} qbShaderResourcePipelineLayoutAttr_, *qbShaderResourcePipelineLayoutAttr;

typedef struct qbShaderResourcePipelineLayout_* qbShaderResourcePipelineLayout;
QB_API void qb_shaderresourcepipelinelayout_create(qbShaderResourcePipelineLayout* layout, qbShaderResourcePipelineLayoutAttr attr);

typedef struct qbShaderResourceSetAttr_ {
  uint32_t create_count;
  qbShaderResourceLayout layout;
} qbShaderResourceSetAttr_, *qbShaderResourceSetAttr;

typedef struct qbShaderResourceSet_* qbShaderResourceSet;
QB_API void qb_shaderresourceset_create(qbShaderResourceSet* resource_set, qbShaderResourceSetAttr attr);

typedef struct qbWriteShaderResource_ {
  qbShaderResourceSet dst_set;
  uint32_t dst_binding;
  uint32_t dst_array_element;
} qbWriteShaderResource_, *qbWriteShaderResource;
QB_API void qb_shaderresourceset_write(qbShaderResourceSet resource_set, uint32_t write_count, qbWriteShaderResource writes);
QB_API void qb_shaderresourceset_writeuniform(qbShaderResourceSet resource_set, uint32_t binding, qbGpuBuffer buffer);
QB_API void qb_shaderresourceset_writeimage(qbShaderResourceSet resource_set, uint32_t binding, qbImage image, qbImageSampler sampler);

typedef struct qbRenderPipelineAttr_ {
  const char* name;

  qbShaderModule shader;
  qbGeometryDescriptor geometry;  
  qbRenderPass render_pass;
  qbColorBlendState blend_state;
  qbViewportState viewport_state;
  qbRasterizationInfo rasterization_info;
  qbShaderResourcePipelineLayout resource_layout;

  qbShaderResourceBinding resources;
  size_t resources_count;

  qbRenderExt ext;
} qbRenderPipelineAttr_, *qbRenderPipelineAttr;

typedef struct {
  const char* vs;
  const char* fs;
  const char* gs;

  // If true, interprets vs, fs, gs as literal shader strings.
  qbBool interpret_as_strings;
} qbShaderModuleAttr_, *qbShaderModuleAttr;

typedef struct qbSurfaceAttr_ {
  uint32_t width;
  uint32_t height;

  const char* vs;
  const char* fs;

  qbShaderResourceBinding resources;
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
  qbDrawMode mode;
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

QB_API qbPixelMap qb_pixelmap_create(uint32_t width, uint32_t height, uint32_t depth,
                                     qbPixelFormat format, void* pixels);
QB_API qbPixelMap qb_pixelmap_load(qbPixelFormat format, const char* file);
QB_API void qb_pixelmap_destroy(qbPixelMap* pixels);

QB_API uint32_t qb_pixelmap_width(qbPixelMap pixels);
QB_API uint32_t qb_pixelmap_height(qbPixelMap pixels);
QB_API uint32_t qb_pixelmap_depth(qbPixelMap pixels);
QB_API size_t qb_pixelmap_size(qbPixelMap pixels);
QB_API size_t qb_pixelmap_pixelsize(qbPixelMap pixels);
QB_API qbPixelFormat qb_pixelmap_format(qbPixelMap pixels);

QB_API uint8_t* qb_pixelmap_pixels(qbPixelMap pixels);
QB_API uint8_t* qb_pixelmap_at(qbPixelMap pixels, int32_t x, int32_t y, int32_t z);
QB_API qbResult qb_pixelmap_drawto(qbPixelMap src, qbPixelMap dest, vec4s src_rect, vec4s dest_rect);

QB_API void qb_renderpipeline_create(qbRenderPipeline* pipeline, qbRenderPipelineAttr attr);
QB_API void qb_renderpipeline_destroy(qbRenderPipeline* pipeline);
QB_API void qb_renderpipeline_present(qbRenderPipeline render_pipeline, qbFrameBuffer frame_buffer,
                                      qbRenderEvent event);
QB_API void qb_renderpipeline_resize(qbRenderPipeline render_pipeline, qbViewport_ viewport);
QB_API void qb_renderpipeline_append(qbRenderPipeline pipeline, qbRenderPass pass);
QB_API void qb_renderpipeline_prepend(qbRenderPipeline pipeline, qbRenderPass pass);
QB_API size_t qb_renderpipeline_passes(qbRenderPipeline pipeline, qbRenderPass** passes);
QB_API void qb_renderpipeline_update(qbRenderPipeline pipeline, size_t count, qbRenderPass* pass);
QB_API size_t qb_renderpipeline_remove(qbRenderPipeline pipeline, qbRenderPass pass);

QB_API void qb_framebuffer_create(qbFrameBuffer* frame_buffer, qbFrameBufferAttr attr);
QB_API void qb_framebuffer_destroy(qbFrameBuffer* frame_buffer);
QB_API qbImage qb_framebuffer_gettarget(qbFrameBuffer frame_buffer, size_t i);
QB_API void qb_framebuffer_settarget(qbFrameBuffer frame_buffer, size_t i, qbImage image);

QB_API size_t qb_framebuffer_rendertargets(qbFrameBuffer frame_buffer, qbImage** targets);
QB_API qbImage qb_framebuffer_depthtarget(qbFrameBuffer frame_buffer);
QB_API qbImage qb_framebuffer_stenciltarget(qbFrameBuffer frame_buffer);
QB_API qbImage qb_framebuffer_depthstenciltarget(qbFrameBuffer frame_buffer);
QB_API void qb_framebuffer_resize(qbFrameBuffer frame_buffer, uint32_t width, uint32_t height);
QB_API uint32_t qb_framebuffer_width(qbFrameBuffer frame_buffer);
QB_API uint32_t qb_framebuffer_height(qbFrameBuffer frame_buffer);
QB_API uint32_t qb_framebuffer_readpixel(qbFrameBuffer frame_buffer, uint32_t attachment_binding, int32_t x, int32_t y);
/*QB_API void qb_framebuffer_blit(qbFrameBuffer src, qbFrameBuffer dest,
                                qbViewport_ src_viewport,
                                qbViewport_ dest_viewport,
                                qbFrameBufferAttachment mask, qbFilterType filter);*/

QB_API void qb_renderpass_create(qbRenderPass* render_pass, qbRenderPassAttr attr);
QB_API void qb_renderpass_destroy(qbRenderPass* render_pass);
QB_API void qb_renderpass_draw(qbRenderPass render_pass, qbFrameBuffer frame_buffer);
QB_API void qb_renderpass_drawto(qbRenderPass render_pass, qbFrameBuffer frame_buffer, size_t count, qbRenderGroup* groups);
QB_API void qb_renderpass_resize(qbRenderPass render_pass, qbViewport_ viewport);
QB_API const char* qb_renderpass_name(qbRenderPass render_pass);
QB_API qbRenderExt qb_renderpass_ext(qbRenderPass render_pass);

QB_API qbGeometryDescriptor qb_renderpass_geometry(qbRenderPass render_pass);
QB_API size_t qb_renderpass_bindings(qbRenderPass render_pass, qbBufferBinding* bindings);
QB_API size_t qb_renderpass_groups(qbRenderPass render_pass, qbRenderGroup** groups);
QB_API size_t qb_renderpass_resources(qbRenderPass render_pass, qbShaderResourceBinding* resources);
QB_API size_t qb_renderpass_attributes(qbRenderPass render_pass, qbVertexAttribute* attributes);
QB_API size_t qb_renderpass_uniforms(qbRenderPass render_pass, uint32_t** bindings, qbGpuBuffer** uniforms);
QB_API size_t qb_renderpass_samplers(qbRenderPass render_pass, uint32_t** bindings, qbImageSampler** samplers);
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
QB_API int qb_image_width(qbImage image);
QB_API int qb_image_height(qbImage image);
QB_API qbImageView qb_image_view(qbImage image);

QB_API void qb_gpubuffer_create(qbGpuBuffer* buffer, qbGpuBufferAttr attr);
QB_API void qb_gpubuffer_destroy(qbGpuBuffer* buffer);
QB_API const char* qb_gpubuffer_name(qbGpuBuffer buffer);
QB_API qbRenderExt qb_gpubuffer_ext(qbGpuBuffer buffer);
QB_API void qb_gpubuffer_update(qbGpuBuffer buffer, intptr_t offset, size_t size, void* data);
QB_API void qb_gpubuffer_copy(qbGpuBuffer dst, qbGpuBuffer src, intptr_t dst_offset, intptr_t src_offset, size_t size);
QB_API void qb_gpubuffer_swap(qbGpuBuffer a, qbGpuBuffer b);
QB_API size_t qb_gpubuffer_size(qbGpuBuffer buffer);
QB_API void qb_gpubuffer_resize(qbGpuBuffer buffer, size_t new_size);

QB_API void qb_meshbuffer_create(qbMeshBuffer* buffer, qbMeshBufferAttr attr);
QB_API void qb_meshbuffer_destroy(qbMeshBuffer* buffer);
QB_API const char* qb_meshbuffer_name(qbMeshBuffer buffer);
QB_API qbRenderExt qb_meshbuffer_ext(qbMeshBuffer buffer);
QB_API void qb_meshbuffer_attachvertices(qbMeshBuffer buffer, qbGpuBuffer vertices[], size_t count);
QB_API void qb_meshbuffer_attachindices(qbMeshBuffer buffer, qbGpuBuffer indices, size_t count);
QB_API void qb_meshbuffer_attachimages(qbMeshBuffer buffer, size_t count, uint32_t bindings[], qbImage images[]);
QB_API void qb_meshbuffer_attachuniforms(qbMeshBuffer buffer, size_t count, uint32_t bindings[], qbGpuBuffer uniforms[]);
QB_API size_t qb_meshbuffer_vertices(qbMeshBuffer buffer, qbGpuBuffer** vertices);
QB_API size_t qb_meshbuffer_images(qbMeshBuffer buffer, qbImage** images);
QB_API size_t qb_meshbuffer_uniforms(qbMeshBuffer buffer, qbGpuBuffer** uniforms);
QB_API void qb_meshbuffer_indices(qbMeshBuffer buffer, qbGpuBuffer* indices);
QB_API void qb_meshbuffer_updateimages(qbMeshBuffer buffer, size_t count, uint32_t bindings[], qbImage images[]);

// If the buffer has an index buffer attached, then count will be the number
// of indices used to draw. Otherwise, count will be the number of vertices to
// draw.
QB_API void qb_meshbuffer_setcount(qbMeshBuffer buffer, size_t count);
QB_API size_t qb_meshbuffer_getcount(qbMeshBuffer buffer);

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
QB_API size_t qb_rendergroup_images(qbRenderGroup group, qbImage** images);
QB_API size_t qb_rendergroup_uniforms(qbRenderGroup group, qbGpuBuffer** uniforms);
QB_API qbDrawMode qb_rendergroup_drawmode(qbRenderGroup group);

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

typedef struct qbSwapchainAttr_ {
  qbExtent_ extent;
} qbSwapchainAttr_, *qbSwapchainAttr;

typedef struct qbSwapchain_* qbSwapchain;
QB_API void qb_swapchain_create(qbSwapchain* swapchain, qbSwapchainAttr attr);
QB_API void qb_swapchain_images(qbSwapchain swapchain, size_t* count, qbImage* images);
QB_API uint32_t qb_swapchain_waitforframe(qbSwapchain swapchain);

typedef struct qbBeginRenderPassInfo_ {
  qbRenderPass render_pass;
  qbFrameBuffer framebuffer;
  qbClearValue clear_values;
  uint32_t clear_values_count;
} qbBeginRenderPassInfo_, *qbBeginRenderPassInfo;

typedef struct qbDrawCommandBufferAttr_ {
  uint64_t count;
  struct qbMemoryAllocator_* allocator;
} qbDrawCommandBufferAttr_, *qbDrawCommandBufferAttr;


typedef struct qbDrawCommandBuffer_* qbDrawCommandBuffer;

QB_API void qb_drawcmd_create(qbDrawCommandBuffer* cmd_buf, qbDrawCommandBufferAttr attr);
QB_API void qb_drawcmd_destroy(qbDrawCommandBuffer* cmd_buf, size_t count);
QB_API void qb_drawcmd_clear(qbDrawCommandBuffer cmd_buf);

QB_API void qb_drawcmd_setcull(qbDrawCommandBuffer cmd_buf, qbFace cull_face);
QB_API void qb_drawcmd_beginpass(qbDrawCommandBuffer cmd_buf, qbBeginRenderPassInfo begin_info);
QB_API void qb_drawcmd_endpass(qbDrawCommandBuffer cmd_buf);
QB_API void qb_drawcmd_setviewport(qbDrawCommandBuffer cmd_buf, qbViewport viewport);
QB_API void qb_drawcmd_setscissor(qbDrawCommandBuffer cmd_buf, qbRect rect);
QB_API void qb_drawcmd_bindpipeline(qbDrawCommandBuffer cmd_buf, qbRenderPipeline pipeline);
QB_API void qb_drawcmd_bindshaderresourceset(qbDrawCommandBuffer cmd_buf, qbShaderResourcePipelineLayout layout, uint32_t resource_set_count, qbShaderResourceSet* resource_sets);
QB_API void qb_drawcmd_bindvertexbuffers(qbDrawCommandBuffer cmd_buf, uint32_t first_binding, uint32_t binding_count, qbGpuBuffer* buffers);
QB_API void qb_drawcmd_bindindexbuffer(qbDrawCommandBuffer cmd_buf, qbGpuBuffer buffer);
QB_API void qb_drawcmd_draw(qbDrawCommandBuffer cmd_buf, uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance);
QB_API void qb_drawcmd_drawindexed(qbDrawCommandBuffer cmd_buf, uint32_t index_count, uint32_t instance_count, uint32_t vertex_offset, uint32_t first_instance);
QB_API void qb_drawcmd_drawfunc(qbDrawCommandBuffer cmd_buf, void(*func)(qbDrawCommandBuffer, qbVar), qbVar arg);
QB_API void qb_drawcmd_updatebuffer(qbDrawCommandBuffer cmd_buf, qbGpuBuffer buffer, intptr_t offset, size_t size, void* data);
QB_API void qb_drawcmd_pushbuffer(qbDrawCommandBuffer cmd_buf, qbGpuBuffer buffer, intptr_t offset, size_t size, void* data);

typedef struct qbDrawPresentInfo_ {
  qbSwapchain swapchain;
  uint32_t image_index;
} qbDrawPresentInfo_, *qbDrawPresentInfo;

QB_API qbTask qb_drawcmd_present(qbDrawCommandBuffer cmd_buf, qbDrawPresentInfo present_info);

#endif  // CUBEZ_RENDER_PIPELINE__H