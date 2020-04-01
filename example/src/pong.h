#include <cubez/cubez.h>

namespace pong
{

struct Settings {
  uint32_t width;
  uint32_t height;
};

void Initialize(Settings settings);

void OnRender(struct qbRenderEvent_* event);

}