#ifndef FORWARD_RENDERER__H
#define FORWARD_RENDERER__H

#include <cubez/cubez.h>
#include <cubez/render_pipeline.h>
#include <cubez/mesh.h>
#include <cubez/render.h>

typedef struct qbForwardRenderer_* qbForwardRenderer;

typedef struct qbForwardRendererAttr_ {
  uint32_t width;
  uint32_t height;

  // A list of any new uniforms to be used in the shader. The bindings should
  // start at 0. These should not include any texture sampler uniforms. For
  // those, use the image_samplers value.
  qbShaderResourceInfo shader_resources;
  size_t shader_resource_count;

  // The bindings should start at 0. These should not include any texture
  // sampler uniforms. For those, use the image_samplers value.
  // Unimplemented.
  qbGpuBuffer* uniforms;
  uint32_t* uniform_bindings;
  size_t uniform_count;

  // A list of any new texture samplers to be used in the shader. This will
  // automatically create all necessary qbShaderResourceInfos. Do not create
  // individual qbShaderResourceInfos for the given samplers.
  qbImageSampler* image_samplers;
  size_t image_sampler_count;

} qbForwardRendererAttr_, *qbForwardRendererAttr;

qbResult qb_forwardrenderer_create(qbRenderer* renderer, qbForwardRendererAttr attr);
qbResult qb_forwardrenderer_destroy(qbRenderer* renderer);

#endif