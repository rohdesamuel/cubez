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

typedef struct qbGuiBlock_* qbGuiBlock;
typedef struct qbGuiConstraint_* qbGuiConstraint;

typedef struct {
  bool(*onfocus)(qbGuiBlock block);
  bool(*onclick)(qbGuiBlock block, qbMouseButtonEvent e);
  bool(*onscroll)(qbGuiBlock block, qbMouseScrollEvent e);
  bool(*onmove)(qbGuiBlock block, qbMouseMotionEvent e, int start_x, int start_y);
  bool(*onkey)(qbGuiBlock block, qbKeyEvent e);

  void(*onrender)(qbGuiBlock block);
  void(*onopen)(qbGuiBlock block);
  void(*onclose)(qbGuiBlock block);
  void(*ondestroy)(qbGuiBlock block);

  qbVar(*setvalue)(qbGuiBlock block, qbVar cur_val, qbVar new_val);
  qbVar(*getvalue)(qbGuiBlock block, qbVar val);
  void(*onvaluechange)(qbGuiBlock block, qbVar old_val, qbVar new_val);
} qbGuiBlockCallbacks_, *qbGuiBlockCallbacks;

typedef struct {
  vec4s background_color;
  qbImage background;
  float radius;
  vec2s offset;

  qbGuiBlockCallbacks callbacks;
} qbGuiBlockAttr_, *qbGuiBlockAttr;

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
  vec4s text_color;
  qbTextAlign align;
  const char* font_name;
} qbTextboxAttr_, *qbTextboxAttr;

QB_API void qb_guiconstraint(qbGuiBlock block, qbConstraint constraint, qbGuiProperty property, float val);
QB_API float qb_guiconstraint_get(qbGuiBlock block, qbConstraint constraint, qbGuiProperty property);
QB_API void qb_guiconstraint_x(qbGuiBlock block, qbConstraint constraint, float val);
QB_API void qb_guiconstraint_y(qbGuiBlock block, qbConstraint constraint, float val);
QB_API void qb_guiconstraint_width(qbGuiBlock block, qbConstraint constraint, float val);
QB_API void qb_guiconstraint_height(qbGuiBlock block, qbConstraint constraint, float val);

QB_API void qb_guiconstraint_clear(qbGuiBlock block, qbConstraint constraint, qbGuiProperty property);
QB_API void qb_guiconstraint_clearx(qbGuiBlock block, qbConstraint constraint);
QB_API void qb_guiconstraint_cleary(qbGuiBlock block, qbConstraint constraint);
QB_API void qb_guiconstraint_clearwidth(qbGuiBlock block, qbConstraint constraint);
QB_API void qb_guiconstraint_clearheight(qbGuiBlock block, qbConstraint constraint);

QB_API void qb_guiblock_create(qbGuiBlock* window, qbGuiBlockAttr attr, const char* name);

QB_API void qb_guiblock_open(qbGuiBlock block);
QB_API qbGuiBlock qb_guiblock_find(const char* path[]);
QB_API void qb_guiblock_openquery(const char* path[]);
QB_API void qb_guiblock_openchildren(qbGuiBlock block);
QB_API void qb_guiblock_close(qbGuiBlock block);
QB_API void qb_guiblock_closequery(const char* path[]);
QB_API void qb_guiblock_closechildren(qbGuiBlock block);

QB_API void qb_guiblock_link(qbGuiBlock parent, qbGuiBlock child);
QB_API void qb_guiblock_unlink(qbGuiBlock child);
QB_API qbGuiBlock qb_guiblock_parent(qbGuiBlock block);

QB_API void qb_guiblock_movetofront(qbGuiBlock block);
QB_API void qb_guiblock_movetoback(qbGuiBlock block);

QB_API void qb_guiblock_moveforward(qbGuiBlock block);
QB_API void qb_guiblock_movebackward(qbGuiBlock block);

QB_API void qb_guiblock_moveto(qbGuiBlock block, vec3s pos);
QB_API void qb_guiblock_resizeto(qbGuiBlock block, vec2s size);

QB_API void qb_guiblock_moveby(qbGuiBlock block, vec3s pos_delta);
QB_API void qb_guiblock_resizeby(qbGuiBlock block, vec2s size_delta);

QB_API vec2s qb_guiblock_size(qbGuiBlock block);
QB_API vec2s qb_guiblock_pos(qbGuiBlock block);
QB_API vec2s qb_guiblock_relpos(qbGuiBlock block);

QB_API void qb_guiblock_setvalue(qbGuiBlock block, qbVar val);
QB_API qbVar qb_guiblock_value(qbGuiBlock block);

QB_API void qb_guiblock_text(qbGuiBlock block, const char16_t* text);

QB_API qbGuiBlock qb_guiblock_focus();
QB_API qbGuiBlock qb_guiblock_focusat(int x, int y);

QB_API void qb_gui_subscribe(const char* path[], qbGuiBlockCallbacks callbacks, qbVar user_state);

QB_API void qb_textbox_create(qbGuiBlock* window,
                              qbTextboxAttr textbox_attr,
                              const char* name,
                              vec2s size,
                              uint32_t font_size,
                              const char16_t* text);
QB_API void qb_textbox_text(qbGuiBlock block, const char16_t* text);
QB_API void qb_textbox_color(qbGuiBlock block, vec4s text_color);
QB_API void qb_textbox_scale(qbGuiBlock block, vec2s scale);
QB_API void qb_textbox_fontsize(qbGuiBlock block, uint32_t font_size);

QB_API void qb_gui_resize(uint32_t width, uint32_t height);

#endif  // CUBEZ_GUI__H