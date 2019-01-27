#ifndef TEXTURE_OVERLAY__H
#define TEXTURE_OVERLAY__H

#include <cubez/common.h>
#include <glm/glm.hpp>
#include <Framework/Overlay.h>
#include <Framework/JSHelpers.h>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <string>

#include "gui.h"

namespace cubez
{

// Not thread-safe.
class TextureOverlay : public framework::Overlay {
public:
  struct Properties {
    glm::vec2 size;
    glm::vec2 pos;
    float scale;
  };

  TextureOverlay(const gui::Context& context, Properties&& properties);

  void Bind();

private:
  int32_t texture_id_;
  const float scale_;
};

}

#endif  // TEXTURE_OVERLAY__H