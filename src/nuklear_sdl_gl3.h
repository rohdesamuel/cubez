#ifndef NUKLEAR_SDL_GL3__H
#define NUKLEAR_SDL_GL3__H

struct nk_context*   nk_sdl_init(struct SDL_Window* win);
struct nk_context*   nk_sdl_ctx();
void                 nk_sdl_font_stash_begin(struct nk_font_atlas** atlas);
void                 nk_sdl_font_stash_end(void);
int                  nk_sdl_handle_event(union SDL_Event* evt);
void                 nk_sdl_render(enum nk_anti_aliasing AA, int max_vertex_buffer, int max_element_buffer);
void                 nk_sdl_shutdown(void);
void                 nk_sdl_device_destroy(void);
void                 nk_sdl_device_create(void);


#endif  // NUKLEAR_SDL_GL3__H