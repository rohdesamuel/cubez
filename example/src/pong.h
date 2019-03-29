#include "render.h"
#include "render_module.h"

namespace pong
{

struct Settings {
  qbRenderPass scene_pass;
  uint32_t width;
  uint32_t height;
};

void Initialize(Settings settings);

void OnRender(qbRenderEvent event);

}