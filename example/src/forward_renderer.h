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

qbRenderer qb_forwardrenderer_create(uint32_t width, uint32_t height, struct qbRendererAttr_* args);
void qb_forwardrenderer_destroy(qbRenderer renderer);

#endif