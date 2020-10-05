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

#include "font_render.h"
#include "utf8.h"

FontRender::FontRender(Font* font) : font_(font) {}


std::vector<FontRender::FormatChar> FontRender::ParseToFormatString(
    const utf8_t* text, qbTextAlign align, vec2s bounding_size, vec2s font_scale) {

  std::vector<FontRender::FormatChar> ret;

  if (!text) {
    return ret;
  }

  size_t len = u8_strlen((char*)text);

  ret.reserve(len);

  float line_bitmapwidth = 0.f;
  float break_bitmapwidth = 0.f;
  int64_t previous_breakpoint = -1;
  bool replace_breakpoint = true;

  int i = 0;
  utf8_t* s = (utf8_t*)text;
  uint32_t uni_c;
  while ((uni_c = u8_nextchar(s, &i)) != 0) {
    FormatChar c = { uni_c, (*font_)[uni_c], Format::CHAR };

    float advance = c.info.ax * font_scale.x;
    if (uni_c == U'\n') {
      line_bitmapwidth = 0.f;
      break_bitmapwidth = 0.f;
      c.f = Format::NEWLINE;
    } else if (uni_c == U' ') {
      line_bitmapwidth += advance;
      break_bitmapwidth = 0.f;
      previous_breakpoint = ret.size();
      c.f = Format::SPACE;
    } else if (uni_c == U'.') {
      line_bitmapwidth += advance;
      break_bitmapwidth = 0.f;
      previous_breakpoint = ret.size();
      c.f = Format::STOP;
    } else {
      line_bitmapwidth += advance;
      break_bitmapwidth += advance;
    }

    // If the character is past the bounding size, then we can try to draw it again at the next line.
    if (line_bitmapwidth > bounding_size.x && previous_breakpoint >= 0) {
      FormatChar newline = {};
      newline.c = u'\n';
      newline.info = (*font_)[u'\n'];
      newline.f = NEWLINE;

      if (c.f == Format::CHAR) {
        ret.push_back(c);
        ret.insert(ret.begin() + previous_breakpoint + 1, newline);
      } else if (c.f == Format::STOP) {
        break_bitmapwidth = 0.f;
        for (int64_t i = (int64_t)ret.size() - 1; i >= 0; --i) {
          break_bitmapwidth += ret[i].info.ax * font_scale.x;
          if (ret[i].f == Format::SPACE) {            
            ret[i] = newline;
            break;
          }
        }
        ret.push_back(c);
      } else {
        ret.push_back(c);
        ret.push_back(newline);
      }

      // Reset the state to the next line.
      line_bitmapwidth = break_bitmapwidth;
    } else {
      ret.push_back(c);
    }
  }

  return ret;
}

// Todo: improve with decomposed signed distance fields: 
// https://gamedev.stackexchange.com/questions/150704/freetype-create-signed-distance-field-based-font
void FontRender::Render(const utf8_t* text, qbTextAlign align, vec2s bounding_size, vec2s font_scale,
                        std::vector<float>* vertices, std::vector<int>* indices) {
  if (!text) {
    return;
  }

  auto format_string = ParseToFormatString(text, align, bounding_size, font_scale);
  float x = 0;
  float y = 0;
  int index = 0;
  size_t line_start = 0;
  float line_width = 0.0f;
  float line_bitmapwidth = 0.0f;
  float line_final_char_width = 0.0f;
  Character prev_char = {};
  
  for (auto& c : format_string) {
    if (c.f == Format::NEWLINE) {
      float offset = 0.0f;
      if (align == QB_TEXT_ALIGN_CENTER) {
        offset = bounding_size.x * 0.5f - line_bitmapwidth * 0.5f + (prev_char.ax - prev_char.bx - prev_char.bw) * 0.5f;
      } else if (align == QB_TEXT_ALIGN_RIGHT) {
        offset = bounding_size.x - line_bitmapwidth + (prev_char.ax - prev_char.bx - prev_char.bw);
      }
      for (size_t j = line_start; j < vertices->size(); j += 8) {
        (*vertices)[j] += offset;
        (*vertices)[j] = std::round((*vertices)[j]);
      }

      x = 0;
      y -= font_->FontHeight();
      line_final_char_width = 0.0f;
      line_width = 0.0f;
      line_bitmapwidth = 0.0f;
      line_start = vertices->size();
      prev_char = c.info;
      continue;
    }

    if (c.f == Format::SPACE) {
      x += c.info.ax * font_scale.x;
      line_width += c.info.ax * font_scale.x;
      line_bitmapwidth += c.info.ax * font_scale.x;
      prev_char = c.info;
      continue;
    }

    float l = x + c.info.bl * font_scale.x;
    float t = -y - c.info.bt * font_scale.y + font_->AtlasHeight();
    float w = c.info.bw * font_scale.x;
    float h = c.info.bh * font_scale.y;
    float r = l + w;
    float b = t + h;
    float txl = c.info.tx;
    float txr = c.info.tx + c.info.bw / font_->AtlasWidth();
    float txt = 0.0f;
    float txb = c.info.bh / font_->AtlasHeight();

    line_width += (c.info.bw + c.info.bx) * font_scale.x;
    line_bitmapwidth += c.info.ax * font_scale.x;

    if (!w || !h) {
      continue;
    }

    float verts[] = {
      // Positions  // Colors          // UVs
      l, t, 0.0f,   1.0f, 0.0f, 0.0f,  txl, txt,
      r, t, 0.0f,   0.0f, 1.0f, 0.0f,  txr, txt,
      r, b, 0.0f,   0.0f, 0.0f, 1.0f,  txr, txb,
      l, b, 0.0f,   1.0f, 0.0f, 1.0f,  txl, txb,
    };
    vertices->insert(vertices->end(), verts, verts + (sizeof(verts) / sizeof(float)));

    int indx[] = {
      index + 3, index + 1, index + 0,
      index + 3, index + 2, index + 1
    };
    indices->insert(indices->end(), indx, indx + (sizeof(indx) / sizeof(int)));
    index += 4;

    /* Advance the cursor to the start of the next character */
    x += c.info.ax * font_scale.x;
    line_final_char_width = (c.info.bw + c.info.bx) * font_scale.x;
    y += c.info.ay * font_scale.y;
    prev_char = c.info;
  }
  float offset = 0.0f;
  if (align == QB_TEXT_ALIGN_CENTER) {
    offset = bounding_size.x * 0.5f - line_bitmapwidth * 0.5f + (prev_char.ax - prev_char.bx - prev_char.bw)*0.5f;
  } else if (align == QB_TEXT_ALIGN_RIGHT) {
    offset = bounding_size.x - line_bitmapwidth + (prev_char.ax - prev_char.bx - prev_char.bw);
  }
  for (size_t j = line_start; j < vertices->size(); j += 8) {
    (*vertices)[j] += offset;
    (*vertices)[j] = std::round((*vertices)[j]);
  }
}