#include "font_render.h"

FontRender::FontRender(Font* font): font_(font) { }

// Todo: improve with decomposed signed distance fields: 
// https://gamedev.stackexchange.com/questions/150704/freetype-create-signed-distance-field-based-font
void FontRender::Render(const char16_t* text, qbTextAlign align, vec2s bounding_size, vec2s font_scale,
                        std::vector<float>* vertices, std::vector<int>* indices) {
  if (!text) {
    return;
  }

  size_t len = 0;
  for (size_t i = 0; i < 1 << 10; ++i) {
    if (text[i] == 0) {
      break;
    }
    ++len;
  }

  float x = 0;
  float y = 0;
  int index = 0;
  size_t line_start = 0;
  float line_width = 0.0f;
  float line_bitmapwidth = 0.0f;
  float line_final_char_width = 0.0f;
  Character prev_char = {};
  for (size_t i = 0; i < len; ++i) {
    Character& c = (*font_)[text[i]];
    if (text[i] == '\n') {
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
      prev_char = c;
      continue;
    }
    if (text[i] == ' ') {
      x += c.ax * font_scale.x;
      line_width += c.ax * font_scale.x;
      line_bitmapwidth += c.ax * font_scale.x;
      prev_char = c;
      continue;
    }

    float l = x + c.bl * font_scale.x;
    float t = -y - c.bt * font_scale.y + font_->AtlasHeight();
    float w = c.bw * font_scale.x;
    float h = c.bh * font_scale.y;
    float r = l + w;
    float b = t + h;
    float txl = c.tx;
    float txr = c.tx + c.bw / font_->AtlasWidth();
    float txt = 0.0f;
    float txb = c.bh / font_->AtlasHeight();

    line_width += (c.bw + c.bx) * font_scale.x;
    line_bitmapwidth += c.ax * font_scale.x;
    if (line_bitmapwidth > bounding_size.x) {
      float offset = 0.0f;
      if (align == QB_TEXT_ALIGN_CENTER) {
        offset = bounding_size.x * 0.5f - line_bitmapwidth * 0.5f + (prev_char.ax - prev_char.bx - prev_char.bw)*0.5f + c.ax *0.5f;
      } else if (align == QB_TEXT_ALIGN_RIGHT) {
        //offset = size.x - line_width - line_final_char_width + c.ax * scale.x;
        offset = bounding_size.x - line_bitmapwidth + (prev_char.ax - prev_char.bx - prev_char.bw) + c.ax;
      }
      for (size_t j = line_start; j < vertices->size(); j += 8) {
        (*vertices)[j] += offset;
        (*vertices)[j] = std::round((*vertices)[j]);
      }

      x = 0;
      y -= font_->FontHeight();
      line_final_char_width = (c.bw + c.bx) * font_scale.x;
      line_width = 0.0f;
      line_bitmapwidth = 0.0f;
      line_start = vertices->size();
      i -= 1;
      prev_char = c;
      continue;
    }

    if (!w || !h) {
      continue;
    }

    float verts[] = {
      // Positions        // Colors          // UVs
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
    x += c.ax * font_scale.x;
    line_final_char_width = (c.bw + c.bx) * font_scale.x;
    y += c.ay * font_scale.y;
    prev_char = c;
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