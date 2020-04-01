#ifndef CUBEZ_GUI__H
#define CUBEZ_GUI__H

#include <cubez/cubez.h>
#include <cubez/input.h>
#include <cubez/render.h>
#include <cubez/render_pipeline.h>
#include <glm/glm.hpp>

#undef max
#undef min

typedef struct qbWindow_* qbWindow;
typedef struct qbRenderTargetAttr_* qbRenderTargetAttr;
typedef struct qbWindow_* qbWindow;

typedef struct {
  bool(*onfocus)(qbWindow window);
  bool(*onclick)(qbWindow window, qbMouseButtonEvent e);
  bool(*onscroll)(qbWindow window, qbMouseScrollEvent e);
  bool(*onkey)(qbWindow window, qbKeyEvent e);

  void(*onrender)(qbWindow window);
  void(*onopen)(qbWindow window);
  void(*onclose)(qbWindow window);
  void(*ondestroy)(qbWindow window);
} qbWindowCallbacks_, *qbWindowCallbacks;

typedef struct {
  glm::vec4 background_color;
  qbImage background;
  qbWindowCallbacks callbacks;
} qbWindowAttr_, *qbWindowAttr;

typedef enum {
  QB_TEXT_ALIGN_LEFT,
  QB_TEXT_ALIGN_CENTER,
  QB_TEXT_ALIGN_RIGHT,
} qbTextAlign;

typedef struct {
  glm::vec4 text_color;
  qbTextAlign align;
  const char* font_name;
} qbTextboxAttr_, *qbTextboxAttr;

QB_API void qb_window_create(qbWindow* window, qbWindowAttr attr, glm::vec2 pos,
                      glm::vec2 size, qbWindow parent, bool open);

QB_API void qb_textbox_create(qbWindow* window,
                       qbTextboxAttr textbox_attr,
                       glm::vec2 pos, glm::vec2 size, qbWindow parent, bool open,
                       uint32_t font_size,
                       const char16_t* text);
QB_API void qb_textbox_text(qbWindow window, const char16_t* text);
QB_API void qb_textbox_color(qbWindow window, glm::vec4 text_color);
QB_API void qb_textbox_scale(qbWindow window, glm::vec2 scale);
QB_API void qb_textbox_fontsize(qbWindow window, uint32_t font_size);

QB_API void qb_window_open(qbWindow window);
QB_API void qb_window_close(qbWindow window);

QB_API void qb_window_movetofront(qbWindow window);
QB_API void qb_window_movetoback(qbWindow window);

QB_API void qb_window_moveforward(qbWindow window);
QB_API void qb_window_movebackward(qbWindow window);

QB_API void qb_window_moveto(qbWindow window, glm::vec3 pos);
QB_API void qb_window_resizeto(qbWindow window, glm::vec2 size);

QB_API void qb_window_moveby(qbWindow window, glm::vec3 pos_delta);
QB_API void qb_window_resizeby(qbWindow window, glm::vec2 size_delta);

QB_API glm::vec2 qb_window_size(qbWindow window);
QB_API glm::vec2 qb_window_pos(qbWindow window);
QB_API glm::vec2 qb_window_relpos(qbWindow window);
QB_API qbWindow qb_window_parent(qbWindow window);

QB_API qbWindow qb_window_focus();
QB_API qbWindow qb_window_focusat(glm::ivec2 mouse_coord);

#endif  // CUBEZ_GUI__H