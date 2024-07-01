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

#ifndef RENDERER__H
#define RENDERER__H

#include <cubez/cubez.h>
#include <cubez/render_pipeline.h>
#include <cubez/mesh.h>

typedef struct qbRenderer_ {
  void(*render)(struct qbRenderer_* self, qbRenderEvent event);
  void(*resize)(struct qbRenderer_* self, uint32_t width, uint32_t height);

  qbResult(*draw_beginframe)(struct qbRenderer_* self, const struct qbCamera_* camera, qbClearValue clear);
  qbResult(*drawcommands_submit)(struct qbRenderer_* self, size_t count, struct qbDrawCommand_* cmds);
  qbDrawCommandBuffer* (*drawcommands_compile)(struct qbRenderer_* self, size_t count, struct qbDrawCommand_* cmds, struct qbDrawCompileAttr_* attr, uint32_t* frame_count);

  void(*light_enable)(struct qbRenderer_* self, qbId id, enum qbLightType type);
  void(*light_disable)(struct qbRenderer_* self, qbId id, enum qbLightType type);
  bool(*light_isenabled)(struct qbRenderer_* self, qbId id, enum qbLightType type);
  void(*light_directional)(struct qbRenderer_* self, qbId id, vec3s rgb,
    vec3s dir, float brightness);
  void(*light_point)(struct qbRenderer_* self, qbId id, vec3s rgb,
    vec3s pos, float linear, float quadratic, float radius);
  void(*light_spot)(struct qbRenderer_* self, qbId id, vec3s rgb,
    vec3s pos, vec3s dir, float brightness,
    float radius, float angle_deg);
  size_t(*light_max)(struct qbRenderer_* self, enum qbLightType light_type);

  void(*mesh_create)(struct qbRenderer_* self, struct qbMesh_* mesh);
  void(*mesh_destroy)(struct qbRenderer_* self, struct qbMesh_* mesh);

  const char* title;
  void* state;
} qbRenderer_, * qbRenderer;

typedef struct qbRendererAttr_ {
  struct qbRenderer_* (*create_renderer)(uint32_t width, uint32_t height, struct qbRendererAttr_* args);
  void(*destroy_renderer)(struct qbRenderer_* renderer);
} qbRendererAttr_, * qbRendererAttr;

typedef struct qbDefaultRenderer_* qbDefaultRenderer;

typedef struct qbDefaultRendererAttr_ {
  uint32_t width;
  uint32_t height;
} qbDefaultRendererAttr_, * qbDefaultRendererAttr;

qbRenderer qb_defaultrenderer_create(uint32_t width, uint32_t height, struct qbRendererAttr_* args);
void qb_defaultrenderer_destroy(qbRenderer renderer);

QB_API qbRenderer qb_renderer();

#endif