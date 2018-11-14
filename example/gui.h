#ifndef GUI__H
#define GUI__H

#include "stb_image.h"

namespace gui {

struct Settings {
  const char* asset_dir;
  int width;
  int height;
};

void Initialize(SDL_Window* win, Settings settings);

void Shutdown();

void Render();

void HandleInput(SDL_Event* e);

SDL_Window* Window();

}

#endif  // GUI__H