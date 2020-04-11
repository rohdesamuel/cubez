#ifndef FONT_RENDER__H
#define FONT_RENDER__H

#include "font_registry.h"
#include <cubez/gui.h>

class FontRender {
public:
  FontRender(Font* font);
  void Render(const char16_t* text, qbTextAlign align,
              vec2s bounding_size, vec2s font_scale,
              std::vector<float>* vertices, std::vector<int>* indices);

private:
  Font* font_;
};

#endif  // FONT_RENDER__H