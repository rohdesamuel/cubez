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
#include <cubez/render.h>

typedef struct qbDefaultRenderer_* qbDefaultRenderer;

typedef struct qbDefaultRendererAttr_ {
  uint32_t width;
  uint32_t height;
} qbDefaultRendererAttr_, * qbDefaultRendererAttr;

qbRenderer qb_defaultrenderer_create(uint32_t width, uint32_t height, struct qbRendererAttr_* args);
void qb_defaultrenderer_destroy(qbRenderer renderer);

#endif