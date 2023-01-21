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

#ifndef CUBEZ_GUI__H
#define CUBEZ_GUI__H

#include <cubez/cubez.h>
#include <cubez/input.h>
#include <cubez/render.h>
#include <cubez/render_pipeline.h>

#undef max
#undef min

typedef struct qbGuiElement_* qbGuiElement;
typedef struct qbGuiConstraint_* qbGuiConstraint;

typedef struct {
  qbBool(*onfocus)(qbGuiElement el);
  qbBool(*onclick)(qbGuiElement el, qbMouseButtonEvent e);
  qbBool(*onscroll)(qbGuiElement el, qbMouseScrollEvent e);
  qbBool(*onmove)(qbGuiElement el, qbMouseMotionEvent e, int start_x, int start_y);
  qbBool(*onkey)(qbGuiElement el, qbKeyEvent e);

  void(*onrender)(qbGuiElement el);
  void(*onopen)(qbGuiElement el);
  void(*onclose)(qbGuiElement el);
  void(*ondestroy)(qbGuiElement el);

  void(*onvaluechange)(qbGuiElement el, qbVar old_val, qbVar new_val);
  qbVar(*onsetvalue)(qbGuiElement el, qbVar cur_val, qbVar new_val);
  qbVar(*ongetvalue)(qbGuiElement el, qbVar val);  
} qbGuiElementCallbacks_, *qbGuiElementCallbacks;

typedef enum qbTextAlign {
  QB_TEXT_ALIGN_LEFT,
  QB_TEXT_ALIGN_CENTER,
  QB_TEXT_ALIGN_RIGHT,
} qbTextAlign;

typedef enum qbGuiProperty {
  QB_GUI_X,
  QB_GUI_Y,
  QB_GUI_WIDTH,
  QB_GUI_HEIGHT,
} qbGuiProperty;

typedef enum qbConstraint {
  QB_CONSTRAINT_NONE,  
  QB_CONSTRAINT_PIXEL,
  QB_CONSTRAINT_PERCENT,
  QB_CONSTRAINT_RELATIVE,
  QB_CONSTRAINT_MIN,
  QB_CONSTRAINT_MAX,
  QB_CONSTRAINT_ASPECT_RATIO
} qbConstraint;

typedef struct {
  vec4s background_color;
  qbImage background;
  float radius;
  vec2s offset;

  vec4s text_color;
  qbTextAlign text_align;
  int text_size;
  const char* font_name;

  qbGuiElementCallbacks callbacks;
  qbVar state;
} qbGuiElementAttr_, *qbGuiElementAttr;

QB_API void qb_guielement_setconstraint(qbGuiElement el, qbGuiProperty property, qbConstraint constraint, float val);
QB_API float qb_guielement_getconstraint(qbGuiElement el, qbGuiProperty property, qbConstraint constraint);
QB_API void qb_guielement_clearconstraint(qbGuiElement el, qbGuiProperty property, qbConstraint constraint);

QB_API void qb_guielement_create(qbGuiElement* el, const char* id,
                                 qbGuiElementAttr attr);
QB_API void qb_guielement_destroy(qbGuiElement* el);

QB_API void qb_guielement_open(qbGuiElement el);
QB_API qbGuiElement qb_guielement_find(const char* id);
QB_API void qb_guielement_close(qbGuiElement el);
QB_API void qb_guielement_closechildren(qbGuiElement el);

QB_API void qb_guielement_link(qbGuiElement parent, qbGuiElement child);
QB_API void qb_guielement_unlink(qbGuiElement child);
QB_API qbGuiElement qb_guielement_parent(qbGuiElement el);

QB_API void qb_guielement_movetofront(qbGuiElement el);
QB_API void qb_guielement_movetoback(qbGuiElement el);

QB_API void qb_guielement_moveforward(qbGuiElement el);
QB_API void qb_guielement_movebackward(qbGuiElement el);

QB_API void qb_guielement_moveto(qbGuiElement el, vec2s pos);
QB_API void qb_guielement_resizeto(qbGuiElement el, vec2s size);

QB_API void qb_guielement_moveby(qbGuiElement el, vec2s pos_delta);
QB_API void qb_guielement_resizeby(qbGuiElement el, vec2s size_delta);

QB_API vec2s qb_guielement_getsize(qbGuiElement el);
QB_API vec2s qb_guielement_getpos(qbGuiElement el);
QB_API vec2s qb_guielement_getrelpos(qbGuiElement el);

QB_API void qb_guielement_setvalue(qbGuiElement el, qbVar val);
QB_API qbVar qb_guielement_getvalue(qbGuiElement el);

QB_API qbVar qb_guielement_getstate(qbGuiElement el);

QB_API const utf8_t* qb_guielement_gettext(qbGuiElement el);
QB_API void qb_guielement_settext(qbGuiElement el, const utf8_t* text);

QB_API qbGuiElement qb_guielement_getfocus();
QB_API qbGuiElement qb_guielement_getfocusat(float x, float y);

QB_API void qb_guielement_settextcolor(qbGuiElement el, vec4s color);
QB_API void qb_guielement_settextscale(qbGuiElement el, vec2s scale);
QB_API void qb_guielement_settextsize(qbGuiElement el, uint32_t size);
QB_API void qb_guielement_settextalign(qbGuiElement el, qbTextAlign align);

QB_API void qb_gui_resize(uint32_t width, uint32_t height);

QB_API struct nk_context* nk_ctx();

#endif  // CUBEZ_GUI__H