#include "texture_overlay.h"

namespace cubez
{

TextureOverlay::TextureOverlay(const gui::Context& context, Properties&& properties) :
  framework::Overlay(*context.renderer, context.driver.get(),
                     properties.size.x, properties.size.y,
                     properties.pos.x, properties.pos.y, properties.scale, true),
                     scale_(properties.scale) {
  texture_id_ = view()->render_target().texture_id;
}

void TextureOverlay::Bind() {
  driver_->BindRenderBuffer(view()->render_target().render_buffer_id);
}

}  // namespace cubez