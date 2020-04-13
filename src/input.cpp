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
std::unordered_map<int, bool> key_states;
std::unordered_map<int, bool> mouse_states;
int mouse_x;
int mouse_y;

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

qbButton button_from_sdl(uint32_t sdl_key) {
  switch (sdl_key) {
    case SDL_BUTTON_LEFT: return QB_BUTTON_LEFT;
    case SDL_BUTTON_MIDDLE: return QB_BUTTON_MIDDLE;
    case SDL_BUTTON_RIGHT: return QB_BUTTON_RIGHT;
    case SDL_BUTTON_X1: return QB_BUTTON_X1;
    case SDL_BUTTON_X2: return QB_BUTTON_X2;
  }
  return QB_BUTTON_LEFT;
}


qbKey keycode_from_sdl(SDL_Keycode sdl_key) {
  switch (sdl_key) {
    case SDLK_SPACE: return QB_KEY_SPACE;
    case SDLK_a: return QB_KEY_A;
    case SDLK_b: return QB_KEY_B;
    case SDLK_c: return QB_KEY_C;
    case SDLK_d: return QB_KEY_D;
    case SDLK_e: return QB_KEY_E;
    case SDLK_f: return QB_KEY_F;
    case SDLK_g: return QB_KEY_G;
    case SDLK_h: return QB_KEY_H;
    case SDLK_i: return QB_KEY_I;
    case SDLK_j: return QB_KEY_J;
    case SDLK_k: return QB_KEY_K;
    case SDLK_l: return QB_KEY_L;
    case SDLK_m: return QB_KEY_M;
    case SDLK_n: return QB_KEY_N;
    case SDLK_o: return QB_KEY_O;
    case SDLK_p: return QB_KEY_P;
    case SDLK_q: return QB_KEY_Q;
    case SDLK_r: return QB_KEY_R;
    case SDLK_s: return QB_KEY_S;
    case SDLK_t: return QB_KEY_T;
    case SDLK_u: return QB_KEY_U;
    case SDLK_v: return QB_KEY_V;
    case SDLK_w: return QB_KEY_W;
    case SDLK_x: return QB_KEY_X;
    case SDLK_y: return QB_KEY_Y;
    case SDLK_z: return QB_KEY_Z;
    case SDLK_LEFTBRACKET: return QB_KEY_LBACKET;
    case SDLK_KP_LEFTBRACE: return QB_KEY_LBRACE;
    case SDLK_LSHIFT: return QB_KEY_LSHIFT;
    case SDLK_LALT: return QB_KEY_LALT;
    case SDLK_LCTRL: return QB_KEY_LCTRL;
    case SDLK_RIGHTBRACKET: return QB_KEY_RBACKET;
    case SDLK_KP_RIGHTBRACE: return QB_KEY_RBRACE;
    case SDLK_RSHIFT: return QB_KEY_RSHIFT;
    case SDLK_RALT: return QB_KEY_RALT;
    case SDLK_RCTRL: return QB_KEY_RCTRL;
    case SDLK_UP: return QB_KEY_UP;
    case SDLK_DOWN: return QB_KEY_DOWN;
    case SDLK_LEFT: return QB_KEY_LEFT;
    case SDLK_RIGHT: return QB_KEY_RIGHT;
    default: return QB_KEY_UNKNOWN;
  }
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

void save_key_state(qbKey key, bool state) {
  qbKeyEvent_ input;
  input.was_pressed = key_states[(int)key];
  input.is_pressed = state;
  input.key = key;

  key_states[(int)key] = state;
}

void qb_handle_input(void(*shutdown_handler)()) {
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    qbInputEvent_ input_event;

    if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
      input_event.type = QB_INPUT_EVENT_KEY;
      input_event.key_event.is_pressed = e.key.state == SDL_PRESSED;
      input_event.key_event.key = keycode_from_sdl(e.key.keysym.sym);

      save_key_state(input_event.key_event.key, input_event.key_event.is_pressed);
      input_event.key_event.was_pressed = key_states[(int)input_event.key_event.key];

      qb_send_key_event(&input_event.key_event);
    } else if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) {
      input_event.type = QB_INPUT_EVENT_MOUSE;
      input_event.mouse_event.type = QB_MOUSE_EVENT_BUTTON;
      input_event.mouse_event.button.button = button_from_sdl(e.button.button);
      input_event.mouse_event.button.state = e.button.state ? QB_MOUSE_DOWN : QB_MOUSE_UP;
      qb_send_mouse_click_event(&input_event.mouse_event.button);
    } else if (e.type == SDL_MOUSEMOTION) {
      if (SDL_GetRelativeMouseMode()) {
        input_event.type = QB_INPUT_EVENT_MOUSE;
        input_event.mouse_event.type = QB_MOUSE_EVENT_MOTION;
        input_event.mouse_event.motion.x = e.motion.x;
        input_event.mouse_event.motion.y = e.motion.y;
        input_event.mouse_event.motion.xrel = e.motion.xrel;
        input_event.mouse_event.motion.yrel = e.motion.yrel;
        qb_send_mouse_move_event(&input_event.mouse_event.motion);
      }
    } else if (e.type == SDL_MOUSEWHEEL) {
      input_event.type = QB_INPUT_EVENT_MOUSE;
      input_event.mouse_event.type = QB_MOUSE_EVENT_SCROLL;
      qb_get_mouse_position(&input_event.mouse_event.scroll.x,
                            &input_event.mouse_event.scroll.y);
      input_event.mouse_event.scroll.xrel = e.wheel.x;
      input_event.mouse_event.scroll.yrel = e.wheel.y;
      qb_send_mouse_scroll_event(&input_event.mouse_event.scroll);
    } else if (e.type == SDL_QUIT) {
      shutdown_handler();
    }

    gui_handle_input(&input_event);
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

bool qb_is_key_pressed(qbKey key) {
  return key_states[(int)key];
}

bool qb_is_mouse_pressed(qbButton mouse_button) {
  return (SDL_GetMouseState(nullptr, nullptr) & (1 << (button_to_sdl(mouse_button) - 1))) != 0;
}

void qb_get_mouse_position(int* x, int* y) {
  SDL_GetMouseState(x, y);
}