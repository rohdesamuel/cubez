#ifndef FONT_REGISTRY__H
#define FONT_REGISTRY__H

#include <unordered_map>
#include <memory>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <cubez/render_pipeline.h>

struct Character {
  float ax; // advance.x
  float ay; // advance.y

  float bw; // bitmap.width;
  float bh; // bitmap.rows;

  float bl; // bitmap_left;
  float bt; // bitmap_top;

  float bx; // bearing.x
  float by; // bearing.y

  float tx; // x offset of glyph in texture coordinates
};

class Font {
public:
  Font(FT_Face face, const char* font_name);
  ~Font();

  Character& operator[](char16_t);
  const Character& operator[](char16_t) const;
  qbImage Atlas() const;

  uint32_t FontHeight() const;
  uint32_t AtlasHeight() const;
  uint32_t AtlasWidth() const;

  const char* Name() const;

private:
  const char* font_name_;
  std::unordered_map<char16_t, Character> characters_;
  qbImage font_atlas_;
  uint32_t atlas_width_;
  uint32_t atlas_height_;
  uint32_t font_height_;
};

class FontRegistry {
public:
  FontRegistry();
  void Load(const char* font_file, const char* font_name);
  void Destroy(const char* font_name);
  Font* Get(const char* font_name, uint32_t size);

private:
  struct FontEntry {
    const char* font_file;
    FT_Face face;
    std::unordered_map<uint32_t, std::unique_ptr<Font>> sizes;
  };

  FT_Library library_;
  
  // Font name -> font size -> font
  std::unordered_map<const char*, FontEntry> fonts_;
};

#endif  // FONT_REGISTRY__H