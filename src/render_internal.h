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