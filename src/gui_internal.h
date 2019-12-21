#ifndef GUI_INTERNAL__H
#define GUI_INTERNAL__H

#include <cubez/input.h>

void gui_initialize();
void gui_shutdown();

void gui_handle_input(qbInputEvent input);
qbRenderPass gui_create_renderpass(uint32_t width, uint32_t height);
void gui_window_updateuniforms();

#endif  // GUI_INTERNAL__H