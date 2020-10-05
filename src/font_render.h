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

#ifndef FONT_RENDER__H
#define FONT_RENDER__H

#include "font_registry.h"
#include <cubez/gui.h>

class FontRender {
public:
  FontRender(Font* font);
  void Render(const utf8_t* text, qbTextAlign align,
              vec2s bounding_size, vec2s font_scale,
              std::vector<float>* vertices, std::vector<int>* indices);

private:
  enum Format {
    CHAR,
    NEWLINE,
    SPACE,
    STOP
  };

  struct FormatChar {
    uint32_t c;
    Character info;
    Format f;
  };

  std::vector<FormatChar> ParseToFormatString(
    const utf8_t* text, qbTextAlign align, vec2s bounding_size, vec2s font_scale);

  Font* font_;
};

#endif  // FONT_RENDER__H