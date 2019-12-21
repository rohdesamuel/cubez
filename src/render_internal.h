#ifndef RENDER_INTERNAL__H
#define RENDER_INTERNAL__H

#include <cubez/render_pipeline.h>

struct RenderSettings {
  const char* title;
  int width;
  int height;

  qbRenderer renderer;
};

void render_initialize(RenderSettings* settings);
void renderer_initialize(struct qbRenderer_* renderer);
void render_shutdown();

#endif  // RENDER_INTERNAL__H