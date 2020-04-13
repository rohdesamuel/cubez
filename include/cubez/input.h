/**
* Author: Samuel Rohde (rohde.samuel@cubez.io)
*
* Copyright {2020} {Samuel Rohde}
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

#ifndef INPUT__H
#define INPUT__H

#include <cubez/cubez.h>

#include <functional>

enum qbKey {
  QB_KEY_UNKNOWN,
  QB_KEY_0,
  QB_KEY_1,
  QB_KEY_2,
  QB_KEY_3,
  QB_KEY_4,
  QB_KEY_5,
  QB_KEY_6,
  QB_KEY_7,
  QB_KEY_8,
  QB_KEY_9,
  QB_KEY_A,
  QB_KEY_B,
  QB_KEY_C,
  QB_KEY_D,
  QB_KEY_E,
  QB_KEY_F,
  QB_KEY_G,
  QB_KEY_H,
  QB_KEY_I,
  QB_KEY_J,
  QB_KEY_K,
  QB_KEY_L,
  QB_KEY_M,
  QB_KEY_N,
  QB_KEY_O,
  QB_KEY_P,
  QB_KEY_Q,
  QB_KEY_R,
  QB_KEY_S,
  QB_KEY_T,
  QB_KEY_U,
  QB_KEY_V,
  QB_KEY_W,
  QB_KEY_X,
  QB_KEY_Y,
  QB_KEY_Z,
  QB_KEY_LBACKET,
  QB_KEY_LBRACE,
  QB_KEY_LSHIFT,
  QB_KEY_LALT,
  QB_KEY_LCTRL,
  QB_KEY_RBACKET,
  QB_KEY_RBRACE,
  QB_KEY_RSHIFT,
  QB_KEY_RALT,
  QB_KEY_RCTRL,
  QB_KEY_UP,
  QB_KEY_DOWN,
  QB_KEY_LEFT,
  QB_KEY_RIGHT,
  QB_KEY_SPACE
};

enum qbButton {
  QB_BUTTON_LEFT,
  QB_BUTTON_MIDDLE,
  QB_BUTTON_RIGHT,
  QB_BUTTON_X1,
  QB_BUTTON_X2
};

#define QB_KEY_RELEASED 0
#define QB_KEY_PRESSED 1

enum qbInputEventType {
  QB_INPUT_EVENT_KEY,
  QB_INPUT_EVENT_MOUSE,
};

typedef struct {
  bool was_pressed;
  bool is_pressed;
  qbKey key;
} qbKeyEvent_, *qbKeyEvent;

enum qbMouseState {
  QB_MOUSE_UP,
  QB_MOUSE_DOWN
};

enum qbMouseEventType {
  QB_MOUSE_EVENT_MOTION,
  QB_MOUSE_EVENT_BUTTON,
  QB_MOUSE_EVENT_SCROLL,
};

typedef struct {
  qbMouseState state;
  qbButton button;
} qbMouseButtonEvent_, *qbMouseButtonEvent;

typedef struct {
  int x;
  int y;
  int xrel;
  int yrel;
} qbMouseMotionEvent_, *qbMouseMotionEvent;

typedef struct {
  int x;
  int y;
  int xrel;
  int yrel;
} qbMouseScrollEvent_, *qbMouseScrollEvent;

typedef struct {
  union {
    qbMouseButtonEvent_ button;
    qbMouseMotionEvent_ motion;
    qbMouseScrollEvent_ scroll;
  };
  qbMouseEventType type;
} qbMouseEvent_, *qbMouseEvent;

typedef struct {
  union {
    qbKeyEvent_ key_event;
    qbMouseEvent_ mouse_event;
  };
  qbInputEventType type;
} qbInputEvent_, *qbInputEvent;

QB_API void qb_send_key_event(qbKeyEvent event);
QB_API void qb_send_mouse_click_event(qbMouseButtonEvent event);
QB_API void qb_send_mouse_move_event(qbMouseMotionEvent event);
QB_API void qb_send_mouse_scroll_event(qbMouseScrollEvent event);

QB_API void qb_handle_input(void(*shutdown_handler)());

QB_API qbResult qb_on_key_event(qbSystem system);
QB_API qbResult qb_on_mouse_event(qbSystem system);

QB_API bool qb_is_key_pressed(qbKey key);
QB_API bool qb_is_mouse_pressed(qbButton mouse_button);
QB_API void qb_get_mouse_position(int* x, int* y);

#endif  // INPUT__H

