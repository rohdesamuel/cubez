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

#include <cubez/input.h>
#include <cubez/gui.h>
#include "input_internal.h"
#include "gui_internal.h"
#include <iostream>
#include <unordered_map>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_mouse.h>

qbEvent keyboard_event;
qbEvent mouse_event;
std::unordered_map<int, qbBool> key_states;
std::unordered_map<int, qbBool> scan_states;
std::unordered_map<int, qbBool> mouse_states;
int mouse_x;
int mouse_y;
int mouse_dx;
int mouse_dy;
int wheel_x;
int wheel_y;

volatile qbInputFocus input_focus = QB_FOCUS_APP;

namespace
{
uint32_t button_to_sdl(qbButton button) {
  switch (button) {
    case QB_BUTTON_LEFT: return SDL_BUTTON_LEFT;
    case QB_BUTTON_MIDDLE: return SDL_BUTTON_MIDDLE;
    case QB_BUTTON_RIGHT: return SDL_BUTTON_RIGHT;
    case QB_BUTTON_X1: return SDL_BUTTON_X1;
    case QB_BUTTON_X2: return SDL_BUTTON_X2;
  }
  return SDL_BUTTON_LEFT;
}

qbButton button_from_sdl(uint32_t button) {
  switch (button) {
    case SDL_BUTTON_LEFT: return QB_BUTTON_LEFT;
    case SDL_BUTTON_MIDDLE: return QB_BUTTON_MIDDLE;
    case SDL_BUTTON_RIGHT: return QB_BUTTON_RIGHT;
    case SDL_BUTTON_X1: return QB_BUTTON_X1;
    case SDL_BUTTON_X2: return QB_BUTTON_X2;
  }
  return QB_BUTTON_LEFT;
}


qbKey keycode_from_sdl(SDL_Keycode sdl_key) {
  return (qbKey)sdl_key;
}

qbScanCode scancode_from_sdl(SDL_Scancode sdl_key) {
  return (qbScanCode)sdl_key;
}

}

void input_initialize() {
  mouse_x = 0;
  mouse_y = 0;
  SDL_SetRelativeMouseMode(SDL_TRUE);
  {
    qbEventAttr attr;
    qb_eventattr_create(&attr);
    qb_eventattr_setmessagetype(attr, qbKeyEvent);
    qb_event_create(&keyboard_event, attr);
    qb_eventattr_destroy(&attr);
  }
  {
    qbEventAttr attr;
    qb_eventattr_create(&attr);
    qb_eventattr_setmessagetype(attr, qbMouseEvent);
    qb_event_create(&mouse_event, attr);
    qb_eventattr_destroy(&attr);
  }
}

void save_key_state(qbKey key, qbScanCode scan_code, qbBool state) {
  qbKeyEvent_ input;
  input.was_pressed = key_states[(int)key];
  input.is_pressed = state;
  input.key = key;

  key_states[(int)key] = state;
  scan_states[(int)scan_code] = state;
}

void qb_handle_input(void(*on_shutdown)(qbVar arg), void(*on_resize)(qbVar arg, uint32_t width, uint32_t height), qbVar shutdown_arg, qbVar resize_arg) {
  SDL_Event e;

  SDL_GetRelativeMouseState(&mouse_dx, &mouse_dy);

  // Reset the scroll wheel when it stops moving.
  bool wheel_updated = false;
  while (SDL_PollEvent(&e)) {
    qbInputEvent_ input_event;

    if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
      input_event.type = QB_INPUT_EVENT_KEY;
      input_event.key_event.is_pressed = e.key.state == SDL_PRESSED;
      input_event.key_event.key = keycode_from_sdl(e.key.keysym.sym);
      input_event.key_event.scane_code = scancode_from_sdl(e.key.keysym.scancode);

      save_key_state(input_event.key_event.key, input_event.key_event.scane_code,
                     input_event.key_event.is_pressed);

      input_event.key_event.was_pressed = key_states[(int)input_event.key_event.key];

      qb_send_key_event(&input_event.key_event);
    } else if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) {
      input_event.type = QB_INPUT_EVENT_MOUSE;
      input_event.mouse_event.type = QB_MOUSE_EVENT_BUTTON;
      input_event.mouse_event.button.button = button_from_sdl(e.button.button);
      input_event.mouse_event.button.state = e.button.state ? QB_MOUSE_DOWN : QB_MOUSE_UP;

      mouse_states[input_event.mouse_event.button.button] = input_event.mouse_event.button.state;
    } else if (e.type == SDL_MOUSEMOTION) {
      input_event.type = QB_INPUT_EVENT_MOUSE;
      input_event.mouse_event.type = QB_MOUSE_EVENT_MOTION;
      input_event.mouse_event.motion.x = e.motion.x;
      input_event.mouse_event.motion.y = e.motion.y;
      input_event.mouse_event.motion.xrel = e.motion.xrel;
      input_event.mouse_event.motion.yrel = e.motion.yrel;

      mouse_x = e.motion.x;
      mouse_y = e.motion.y;
      mouse_dx = e.motion.xrel;
      mouse_dy = e.motion.yrel;
    } else if (e.type == SDL_MOUSEWHEEL) {
      wheel_updated = true;
      
      input_event.type = QB_INPUT_EVENT_MOUSE;
      input_event.mouse_event.type = QB_MOUSE_EVENT_SCROLL;
      qb_mouse_getposition(&input_event.mouse_event.scroll.x,
                        &input_event.mouse_event.scroll.y);
      input_event.mouse_event.scroll.xrel = e.wheel.x;
      input_event.mouse_event.scroll.yrel = e.wheel.y;

      wheel_x = e.wheel.x;
      wheel_y = e.wheel.y;
    } else if (e.type == SDL_WINDOWEVENT) {
      if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        qb_window_resize(e.window.data1, e.window.data2);
        on_resize(resize_arg, (uint32_t)e.window.data1, (uint32_t)e.window.data2);
      } else if (e.window.event == SDL_WINDOWEVENT_MAXIMIZED) {
        qb_window_setfullscreen(QB_WINDOW_FULLSCREEN_DESKTOP);
      }
    } else if (e.type == SDL_QUIT) {
      on_shutdown(shutdown_arg);
      return;
    }

    bool handled_by_gui = QB_FOCUS_APP;//gui_handle_input(&input_event) == QB_TRUE;
    /*if (handled_by_gui) {
      input_focus = QB_FOCUS_GUI;
    } else {
      input_focus = QB_FOCUS_APP;
    }*/

    if (qb_user_focus() == QB_FOCUS_APP) {
      if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
        qb_send_key_event(&input_event.key_event);
      } else if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) {
        qb_send_mouse_click_event(&input_event.mouse_event.button);
      } else if (e.type == SDL_MOUSEMOTION) {
        qb_send_mouse_move_event(&input_event.mouse_event.motion);
      } else if (e.type == SDL_MOUSEWHEEL) {
        qb_send_mouse_scroll_event(&input_event.mouse_event.scroll);
      }
    }
  }

  if (!wheel_updated) {
    wheel_x = 0;
    wheel_y = 0;
  }
}

void qb_send_key_event(qbKeyEvent event) {
  qb_event_send(keyboard_event, event);
}

void qb_send_mouse_click_event(qbMouseButtonEvent event) {
  qbMouseEvent_ e;
  e.button = *event;
  e.type = QB_MOUSE_EVENT_BUTTON;
  qb_event_send(mouse_event, &e);
}

void qb_send_mouse_move_event(qbMouseMotionEvent event) {
  qbMouseEvent_ e;
  e.motion = *event;
  e.type = QB_MOUSE_EVENT_MOTION;
  qb_event_send(mouse_event, &e);
}

void qb_send_mouse_scroll_event(qbMouseScrollEvent event) {
  qbMouseEvent_ e;
  e.scroll = *event;
  e.type = QB_MOUSE_EVENT_SCROLL;
  qb_event_send(mouse_event, &e);
}

qbResult qb_on_key_event(qbSystem system) {
  return qb_event_subscribe(keyboard_event, system);
}

qbResult qb_on_mouse_event(qbSystem system) {
  return qb_event_subscribe(mouse_event, system);
}

qbBool qb_scancode_ispressed(qbScanCode scan_code) {
  return scan_states[(int)scan_code];
}

qbBool qb_key_ispressed(qbKey key) {
  return key_states[(int)key];
}

qbBool qb_mouse_ispressed(qbButton mouse_button) {
  return SDL_GetMouseState(nullptr, nullptr) & (1 << (button_to_sdl(mouse_button) - 1));
}

void qb_mouse_getposition(int* x, int* y) {
  SDL_GetMouseState(x, y);
}

void qb_mouse_getrelposition(int* relx, int* rely) {
  *relx = mouse_dx;
  *rely = mouse_dy;  
}

int qb_mouse_setrelative(int enabled) {
  return SDL_SetRelativeMouseMode((SDL_bool)enabled);
}

int qb_mouse_getrelative() {
  return (int)SDL_GetRelativeMouseMode();
}

void qb_mouse_getwheel(int* scroll_x, int* scroll_y) {
  *scroll_x = wheel_x;
  *scroll_y = wheel_y;
}
 

qbInputFocus qb_user_focus() {
  return input_focus;
}