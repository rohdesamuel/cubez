#ifndef GUI__H
#define GUI__H

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
//#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_COMMAND_USERDATA

#include "nuklear/nuklear.h"
#include "nuklear/nuklear_sdl_gl3.h"
#include "stb_image.h"

#define NK_MAX_VERTEX_MEMORY 512 * 1024
#define NK_MAX_ELEMENT_MEMORY 128 * 1024

namespace gui {

void Initialize(SDL_Window* win);

void Shutdown();

void Render();

struct nk_context* Context();
SDL_Window* Window();

}

#endif  // GUI__H