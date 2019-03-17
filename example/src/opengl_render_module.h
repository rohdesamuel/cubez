#ifndef OPENGL_RENDER_MODULE__H
#define OPENGL_RENDER_MODULE__H

#include "render.h"
#include <GL/glew.h>

namespace cubez
{
class CubezGpuDriver;
}

struct qbRenderImpl_ {
  class cubez::CubezGpuDriver* driver;
};

struct qbGpuState_ {
  uint32_t render_buffer_id;
};

qbRenderModule Initialize(uint32_t viewport_width, uint32_t viewport_height, float scale);

#endif  // OPENGL_RENDER_MODULE__H