/**
* Author: Samuel Rohde (rohde.samuel@cubez.io)
*
* Copyright {2020} {Samuel Rohde}
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

#ifndef RENDER_INTERNAL__H
#define RENDER_INTERNAL__H

#include <cubez/render_pipeline.h>

struct RenderSettings {
  const char* title;
  int width;
  int height;

  struct qbRenderer_* (*create_renderer)(uint32_t width, uint32_t height, struct qbRendererAttr_* args);
  void(*destroy_renderer)(struct qbRenderer_* renderer);
  qbRendererAttr_* opt_renderer_args;
};

void render_initialize(RenderSettings* settings);
void render_shutdown();

#endif  // RENDER_INTERNAL__H