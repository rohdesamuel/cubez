#ifndef RENDER__H
#define RENDER__H

#include <string>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <set>
#include <unordered_map>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#include <cubez/cubez.h>
#include "mesh.h"

typedef struct qbRenderPipeline_* qbRenderPipeline;
typedef struct qbRenderable_* qbRenderable;
typedef struct qbRenderModule_* qbRenderModule;
typedef struct qbRenderPass_* qbRenderPass;

// User-defined structs.
typedef struct qbRenderImpl_* qbRenderImpl;
typedef struct qbGpuState_* qbGpuState;
typedef struct qbVertexAttribute_* qbVertexAttribute;
typedef struct qbShaderModule_* qbShaderModule;

typedef struct {
  double alpha;
  uint64_t frame;
} qbRenderEvent;

typedef enum {
  QB_PIXEL_FORMAT_RGBA8
} qbPixelFormat;


typedef enum {
  QB_GPU_BUFFER_TYPE_VERTEX,
  QB_GPU_BUFFER_TYPE_INDEX,
  QB_GPU_BUFFER_TYPE_UNIFORM,
} qbGpuBufferType;
typedef struct qbPixelMap_* qbPixelMap;

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
  qbPixelMap pixels;
} qbImageAttr_, *qbImageAttr;

typedef struct {
  uint32_t id;
  qbPixelMap pixels;
} qbImage_, *qbImage;

typedef enum {
  QB_FILTER_TYPE_NEAREST,
  QB_FILTER_TYPE_LINEAR
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
  qbImageSamplerAttr_ attr;
  uint32_t id;
  const char* name;
  uint32_t binding;
} qbImageSampler_, *qbImageSampler;

typedef struct {
  uint32_t id;
  qbImage render_target;

} qbFrameBuffer_, *qbFrameBuffer;

typedef struct {
  void* data;
  size_t size;
  size_t elem_size;

  qbGpuBufferType buffer_type;
  int sharing_type;
} qbGpuBufferAttr_, *qbGpuBufferAttr;

typedef struct {
  void* data;
  size_t size;
  size_t elem_size;

  uint32_t id;
  qbGpuBufferType buffer_type;
  int sharing_type;
} qbGpuBuffer_, *qbGpuBuffer;

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

typedef struct {
} qbDrawBufferAttr_, *qbDrawBufferAttr;

typedef struct {
  uint32_t id;
  qbGpuBuffer indices;
  
  // Has the same count as bindings_count.
  qbGpuBuffer* vertices;

  std::vector<std::pair<uint32_t, qbGpuBuffer>> uniforms;

  // The index into "images" is the same index into the
  // qbShaderModule.sampler_bindings. This index is also considered the
  // texture unit. This index can get added to GL_TEXTURE0 to bind the texture
  // and sampler.
  qbImage* images;
  size_t images_count;

  qbRenderPass render_pass;
} qbDrawBuffer_, *qbDrawBuffer;

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
} qbShaderResourceAttr_, *qbShaderResourceAttr;

typedef struct {
  const char* vs;
  const char* fs;

  qbShaderResourceAttr resources;
  size_t resources_count;
  
} qbShaderModuleAttr_, *qbShaderModuleAttr;

typedef struct qbShaderModule_ {
  qbShader shader;

  qbShaderResourceAttr resources;
  size_t resources_count;

  std::vector<std::pair<uint32_t, qbGpuBuffer>> uniforms;

  uint32_t* sampler_bindings;
  qbImageSampler* samplers;
  size_t samplers_count;

} qbShaderModule_;

typedef struct qbRenderPass_ {
  uint32_t id;
  qbFrameBuffer frame_buffer;

  std::vector<qbDrawBuffer> draw_buffers;

  qbVertexAttribute attributes;
  size_t attributes_count;

  qbBufferBinding bindings;
  uint32_t bindings_count;

  qbShaderModule shader_program;

  glm::vec4 viewport;
  float viewport_scale;

} qbRenderPass_, *qbRenderPass;

typedef qbDrawBuffer qbVertexBuffer;
typedef qbDrawBuffer qbNormalBuffer;
typedef qbDrawBuffer qbUvBuffer;
typedef qbDrawBuffer qbIndexBuffer;


typedef struct  {
  void(*lockstate)(qbRenderImpl);
  void(*unlockstate)(qbRenderImpl);

  uint32_t(*createtexture)(
    qbRenderImpl,
    qbPixelMap pixel_map);

  void(*updatetexture)(
    qbRenderImpl,
    uint32_t texture_id,
    qbPixelMap pixel_map);

  void(*destroytexture)(
    qbRenderImpl,
    uint32_t texture_id);

  void(*usetexture)(qbRenderImpl,
                    uint32_t texture_id,
                    uint32_t index,
                    uint32_t target);

  uint32_t(*createframebuffer)(
    qbRenderImpl,
    qbFrameBuffer frame_buffer,
    qbFrameBufferAttr attr);

  void(*bindframebuffer)(
    qbRenderImpl,
    uint32_t frame_buffer_id);

  void(*setframebufferviewport)(
    qbRenderImpl,
    uint32_t frame_buffer_id,
    uint32_t width,
    uint32_t height);

  void(*clearframebuffer)(
    qbRenderImpl,
    uint32_t frame_buffer_id);

  void(*destroyframebuffer)(
    qbRenderImpl,
    uint32_t frame_buffer_id);

  uint32_t(*creategeometry)(
    qbRenderImpl,
    const qbVertexBuffer vertices,
    const qbIndexBuffer indices,
    const qbGpuBuffer* buffers, size_t buffer_size,
    const qbVertexAttribute* attributes, size_t attr_size);

  void(*updategeometry)(
    qbRenderImpl,
    uint32_t geometry_id,
    const qbVertexBuffer vertices,
    const qbIndexBuffer indices,
    const qbGpuBuffer* buffers, size_t buffer_size);

  void(*destroygeometry)(
    qbRenderImpl,
    uint32_t geometry_id);

  void(*drawgeometry)(
    qbRenderImpl,
    uint32_t geometry_id,
    size_t indices_count,
    size_t indices_offest);

  uint32_t(*loadprogram)(qbRenderImpl, const char*, const char*);

  void(*useprogram)(qbRenderImpl, uint32_t);

  void(*render)(qbRenderImpl, qbRenderEvent);

  void(*ondestroy)(qbRenderImpl);

} qbRenderInterface_, *qbRenderInterface;


struct Settings {
  std::string title;
  int width;
  int height;
  float fov;
  float znear;
  float zfar;
};

void initialize(const Settings& settings);
void shutdown();
int window_height();
int window_width();

qbRenderable create(qbMesh mesh, qbMaterial material);

qbComponent component();

qbResult qb_render(qbRenderEvent* event);

qbResult qb_camera_setorigin(glm::vec3 new_origin);
qbResult qb_camera_setposition(glm::vec3 new_position);
qbResult qb_camera_setyaw(float new_yaw);
qbResult qb_camera_setpitch(float new_pitch);

qbResult qb_camera_incposition(glm::vec3 delta);
qbResult qb_camera_incyaw(float delta);
qbResult qb_camera_incpitch(float delta);

glm::vec3 qb_camera_getorigin();
glm::vec3 qb_camera_getposition();
glm::mat4 qb_camera_getorientation();
glm::mat4 qb_camera_getprojection();
glm::mat4 qb_camera_getinvprojection();

qbResult qb_camera_screentoworld(glm::vec2 screen, glm::vec3* world);

qbResult qb_render_swapbuffers();
qbResult qb_render_makecurrent();
qbResult qb_render_makenull();

void qb_shadermodule_create(qbShaderModule* shader, qbShaderModuleAttr attr);
void qb_shadermodule_attachuniforms(qbShaderModule module, uint32_t count, uint32_t bindings[], qbGpuBuffer uniforms[]);
void qb_shadermodule_attachsamplers(qbShaderModule module, uint32_t count, uint32_t bindings[], qbImageSampler samplers[]);

float qb_camera_getyaw();
float qb_camera_getpitch();
float qb_camera_getznear();
float qb_camera_getzfar();

qbPixelMap    qb_pixelmap_create(uint32_t width, uint32_t height, qbPixelFormat format);
qbResult      qb_pixelmap_destroy(qbPixelMap* pixels);
uint32_t      qb_pixelmap_width(qbPixelMap pixels);
uint32_t      qb_pixelmap_height(qbPixelMap pixels);
size_t        qb_pixelmap_size(qbPixelMap pixels);
size_t        qb_pixelmap_rowsize(qbPixelMap pixels);
size_t        qb_pixelmap_pixelsize(qbPixelMap pixels);
qbPixelFormat qb_pixelmap_format(qbPixelMap pixels);
void*         qb_pixelmap_pixels(qbPixelMap pixels);
qbResult      qb_pixelmap_drawto(qbPixelMap src, qbPixelMap dest, glm::vec2 src_rect, glm::vec2 dest_rect);


qbResult qb_rendermodule_create(
                     qbRenderModule* module,
                     qbRenderInterface render_interface,
                     qbRenderImpl impl,
                     uint32_t viewport_width,
                     uint32_t viewport_height,
                     float scale);

void qb_renderpipeline_create(qbRenderPipeline* pipeline,
                                   uint32_t viewport_width,
                                   uint32_t viewport_height,
                                   float scale);

void qb_renderpipeline_appendrenderpass(qbRenderPipeline pipeline, qbRenderPass pass);


void qb_framebuffer_create(qbFrameBuffer* frame_buffer, qbFrameBufferAttr attr);
void qb_renderpass_create(qbRenderPass* render_pass, qbRenderPassAttr attr, qbRenderPipeline opt_pipeline);

uint32_t qb_renderpass_drawbuffers(qbRenderPass render_pass, qbDrawBuffer** buffers);
void qb_renderpass_updatedrawbuffers(qbRenderPass render_pass, uint32_t count, qbDrawBuffer* buffers);

void qb_drawbuffer_create(qbDrawBuffer* buffer, qbDrawBufferAttr attr, qbRenderPass render_pass);

void qb_imagesampler_create(qbImageSampler* sampler, qbImageSamplerAttr attr);
void qb_image_create(qbImage* image, qbImageAttr attr);
void qb_image_load(qbImage* image, qbImageAttr attr, const char* file);
void qb_image_update(qbImage image, qbImageType type, qbPixelFormat format, glm::vec3 offset, glm::vec3 sizes);
void qb_image_create(qbImage* image, qbPixelMap pixel_map);

void qb_gpubuffer_create(qbGpuBuffer* buffer, qbGpuBufferAttr);
void qb_gpubuffer_update(qbGpuBuffer buffer, intptr_t offset, size_t size, void* data);
void qb_gpubuffer_copy(qbGpuBuffer dst, qbGpuBuffer src, intptr_t dst_offset, intptr_t src_offset, size_t size);

void qb_drawbuffer_attachvertices(qbDrawBuffer buffer, qbGpuBuffer vertices[]);
void qb_drawbuffer_attachindices(qbDrawBuffer buffer, qbGpuBuffer indices);
void qb_drawbuffer_attachimages(qbDrawBuffer buffer, size_t count, uint32_t bindings[], qbImage images[]);
void qb_drawbuffer_attachuniforms(qbDrawBuffer buffer, uint32_t count, uint32_t bindings[], qbGpuBuffer uniforms[]);

qbResult     qb_rendermodule_destroy(qbRenderModule* module);

void qb_renderpipeline_run(qbRenderPipeline render_pipeline, qbRenderEvent event);

void         qb_rendermodule_drawrenderpass(qbRenderPass render_pass);

qbResult     qb_rendermodule_resize(qbRenderModule module, uint32_t width, uint32_t height);

void         qb_rendermodule_lockstate(qbRenderModule module);

void         qb_rendermodule_unlockstate(qbRenderModule module);

void         qb_rendermodule_updatetexture(qbRenderModule module,
                                               uint32_t texture_id,
                                               qbPixelMap pixel_map);

void        qb_rendermodule_usetexture(qbRenderImpl,
                                           uint32_t texture_id,
                                           uint32_t texture_unit,
                                           uint32_t target);

void         qb_rendermodule_destroytexture(qbRenderModule module,
                                                uint32_t texture_id);

void         qb_rendermodule_drawframebuffer(qbRenderModule module, qbFrameBuffer buffer);


void         qb_rendermodule_bindframebuffer(qbRenderModule module,
                                                  uint32_t frame_buffer_id);

void         qb_rendermodule_setframebufferviewport(qbRenderModule module,
                                                         uint32_t frame_buffer_id,
                                                         uint32_t width,
                                                         uint32_t height);

void         qb_rendermodule_clearframebuffer(qbRenderModule module,
                                                   uint32_t frame_buffer_id);

void         qb_rendermodule_destroyframebuffer(qbRenderModule module,
                                                     uint32_t frame_buffer_id);

uint32_t     qb_rendermodule_creategeometry(qbRenderModule module,
                                                const qbVertexBuffer vertices,
                                                const qbIndexBuffer indices,
                                                const qbGpuBuffer* buffers, size_t buffer_size,
                                                const qbVertexAttribute* attributes, size_t attr_size);

void         qb_rendermodule_updategeometry(qbRenderModule module,
                                                uint32_t geometry_id,
                                                const qbVertexBuffer vertices,
                                                const qbIndexBuffer indices,
                                                const qbGpuBuffer* buffers, size_t buffer_size);

void         qb_rendermodule_destroygeometry(qbRenderModule module,
                                                 uint32_t geometry_id);

void         qb_rendermodule_drawgeometry(qbRenderModule module,
                                              uint32_t geometry_id,
                                              size_t indices_count,
                                              size_t indices_offest);

uint32_t     qb_rendermodule_loadprogram(qbRenderModule module, const char* vs_file, const char* fs_file);

void         qb_rendermodule_useprogram(qbRenderModule module, uint32_t program);

#endif
