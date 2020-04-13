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

#include "font_registry.h"
#include <cubez/common.h>
#include <algorithm>
#include <iostream>

static qbPixelAlignmentOglExt_ alignment_ext;

FontRegistry::FontRegistry() {
  auto error = FT_Init_FreeType(&library_);
  if (error) {
    FATAL("Could not initialize FreeType");
  }

  alignment_ext.ext = {};
  alignment_ext.ext.name = "qbPixelAlignmentOglExt_";
  alignment_ext.alignment = 1;
}

void FontRegistry::Load(const char* font_file, const char* font_name) {
  FT_Face face;
  auto error = FT_New_Face(library_,
                      font_file,
                      0,
                      &face);
  if (error == FT_Err_Unknown_File_Format) {
    FATAL("Unknown file format");
  } else if (error) {
    FATAL("Could not open font");
  }
  fonts_[font_name].font_file = font_file;
  fonts_[font_name].face = face;
}

void FontRegistry::Destroy(const char* font_name) {
  for (auto& font : fonts_[font_name].sizes) {
    font.second.reset();
  }
  FT_Done_Face(fonts_[font_name].face);
  fonts_.erase(font_name);
}

Font* FontRegistry::Get(const char* font_name, uint32_t size) {
  auto& entry = fonts_[font_name];
  auto found = entry.sizes.find(size);
  if (found == entry.sizes.end()) {
    FT_Set_Pixel_Sizes(entry.face, 0, size);
    fonts_[font_name].sizes[size] = std::make_unique<Font>(entry.face, font_name);
    return fonts_[font_name].sizes[size].get();
  }
  return found->second.get();
}

Font::Font(FT_Face face, const char* font_name): font_name_(font_name) {
  FT_GlyphSlot g = face->glyph;

  // Converts 1/64th units to pixel units.
  font_height_ = face->size->metrics.height >> 6;
  //font_height = face->height >> 6;
  unsigned w = 0;
  unsigned h = 0;

  for (int i = 32; i < 128; i++) {
    if (FT_Load_Char(face, i, FT_LOAD_RENDER)) {
      fprintf(stderr, "Loading character %c failed!\n", i);
      continue;
    }

    w += g->bitmap.width;
    h = std::max(h, g->bitmap.rows);
  }

  /* you might as well save this value as it is needed later on */
  atlas_width_ = w;
  atlas_height_ = h;
  qbImageAttr_ atlas = {};
  atlas.name = font_name;
  atlas.type = QB_IMAGE_TYPE_2D;
  atlas.ext = (qbRenderExt)&alignment_ext;
  qb_image_raw(&font_atlas_, &atlas, QB_PIXEL_FORMAT_R8, w, h, nullptr);

  int x = 0;

  for (int i = 32; i < 128; i++) {
    if (FT_Load_Char(face, i, FT_LOAD_RENDER))
      continue;
    Character c = {};
    c.ax = (float)(g->advance.x >> 6);
    c.ay = (float)(g->advance.y >> 6);
    c.bw = (float)g->bitmap.width;
    c.bh = (float)g->bitmap.rows;
    c.bl = (float)g->bitmap_left;
    c.bt = (float)g->bitmap_top;
    c.tx = (float)x / w;
    c.bx = (float)(g->metrics.horiBearingX >> 6);
    c.by = (float)(g->metrics.horiBearingY >> 6);

    characters_[i] = c;

    ivec3s offset = { x, 0, 0 };
    ivec3s sizes = { (int)g->bitmap.width, (int)g->bitmap.rows, 0 };
    qb_image_update(font_atlas_, offset, sizes, g->bitmap.buffer);
    x += g->bitmap.width;
  }
}

Font::~Font() {
  qb_image_destroy(&font_atlas_);
}

Character& Font::operator[](char16_t c) {
  return characters_[c];
}

const Character& Font::operator[](char16_t c) const {
  return characters_.at(c);
}

qbImage Font::Atlas() const {
  return font_atlas_;
}

uint32_t Font::FontHeight() const {
  return font_height_;
}

uint32_t Font::AtlasHeight() const {
  return atlas_height_;
}

uint32_t Font::AtlasWidth() const {
  return atlas_width_;
}

const char* Font::Name() const {
  return font_name_;
}