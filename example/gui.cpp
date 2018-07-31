#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#define STB_IMAGE_IMPLEMENTATION
#define NK_IMPLEMENTATION
#define NK_SDL_GL3_IMPLEMENTATION
#include "gui.h"

namespace gui {

SDL_Window* win;
struct nk_context* context;
struct nk_font_atlas* atlas;

void Initialize(SDL_Window* sdl_window) {
  win = sdl_window;
  context = nk_sdl_init(win);

  nk_sdl_font_stash_begin(&atlas);
  nk_sdl_font_stash_end();
}

void Shutdown() {
  nk_sdl_shutdown();
}

void Render() {
  nk_sdl_render(NK_ANTI_ALIASING_ON, NK_MAX_VERTEX_MEMORY, NK_MAX_ELEMENT_MEMORY);
}

struct nk_context* Context() {
  return context;
}

SDL_Window* Window() {
  return win;
}

}