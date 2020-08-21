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
  QB_KEY_SPACE,
  QB_KEY_ESCAPE,
  QB_KEY_F1,
  QB_KEY_F2,
  QB_KEY_F3,
  QB_KEY_F4,
  QB_KEY_F5,
  QB_KEY_F6,
  QB_KEY_F7,
  QB_KEY_F8,
  QB_KEY_F9,
  QB_KEY_F10,
  QB_KEY_F11,
  QB_KEY_F12,
};

enum qbScanCode {
  QB_SCAN_UNKNOWN,
  QB_SCAN_0,
  QB_SCAN_1,
  QB_SCAN_2,
  QB_SCAN_3,
  QB_SCAN_4,
  QB_SCAN_5,
  QB_SCAN_6,
  QB_SCAN_7,
  QB_SCAN_8,
  QB_SCAN_9,
  QB_SCAN_A,
  QB_SCAN_B,
  QB_SCAN_C,
  QB_SCAN_D,
  QB_SCAN_E,
  QB_SCAN_F,
  QB_SCAN_G,
  QB_SCAN_H,
  QB_SCAN_I,
  QB_SCAN_J,
  QB_SCAN_K,
  QB_SCAN_L,
  QB_SCAN_M,
  QB_SCAN_N,
  QB_SCAN_O,
  QB_SCAN_P,
  QB_SCAN_Q,
  QB_SCAN_R,
  QB_SCAN_S,
  QB_SCAN_T,
  QB_SCAN_U,
  QB_SCAN_V,
  QB_SCAN_W,
  QB_SCAN_X,
  QB_SCAN_Y,
  QB_SCAN_Z,
  QB_SCAN_LBACKET,
  QB_SCAN_LBRACE,
  QB_SCAN_LSHIFT,
  QB_SCAN_LALT,
  QB_SCAN_LCTRL,
  QB_SCAN_RBACKET,
  QB_SCAN_RBRACE,
  QB_SCAN_RSHIFT,
  QB_SCAN_RALT,
  QB_SCAN_RCTRL,
  QB_SCAN_UP,
  QB_SCAN_DOWN,
  QB_SCAN_LEFT,
  QB_SCAN_RIGHT,
  QB_SCAN_SPACE,
  QB_SCAN_ESCAPE,
  QB_SCAN_F1,
  QB_SCAN_F2,
  QB_SCAN_F3,
  QB_SCAN_F4,
  QB_SCAN_F5,
  QB_SCAN_F6,
  QB_SCAN_F7,
  QB_SCAN_F8,
  QB_SCAN_F9,
  QB_SCAN_F10,
  QB_SCAN_F11,
  QB_SCAN_F12,
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
  qbScanCode scane_code;
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

QB_API bool qb_scancode_pressed(qbScanCode scan_code);
QB_API bool qb_key_pressed(qbKey key);
QB_API bool qb_mouse_pressed(qbButton mouse_button);
QB_API bool qb_mouse_waspressed(qbButton mouse_button);

QB_API void qb_mouse_position(int* x, int* y);
QB_API void qb_mouse_relposition(int* relx, int* rely);
QB_API void qb_mouse_wheel(int* scroll_x, int* scroll_y);

QB_API int qb_mouse_setrelative(int enabled);
QB_API int qb_mouse_relative();

typedef enum qbInputFocus {
  QB_FOCUS_APP,
  QB_FOCUS_GUI,
} qbInputFocus;

QB_API qbInputFocus qb_input_focus();

#endif  // INPUT__H