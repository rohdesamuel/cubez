#include "input.h"
#include <unordered_map>

namespace input {

qbEvent keyboard_event;
qbEvent mouse_event;
std::unordered_map<int, bool> key_states;
std::unordered_map<int, bool> mouse_states;
int mouse_x;
int mouse_y;

void initialize() {
  mouse_x = 0;
  mouse_y = 0;
  SDL_SetRelativeMouseMode(SDL_TRUE);
  {
    qbEventAttr attr;
    qb_eventattr_create(&attr);
    qb_eventattr_setmessagetype(attr, InputEvent);
    qb_event_create(&keyboard_event, attr);
    qb_eventattr_destroy(&attr);
  }
  {
    qbEventAttr attr;
    qb_eventattr_create(&attr);
    qb_eventattr_setmessagetype(attr, MouseEvent);
    qb_event_create(&mouse_event, attr);
    qb_eventattr_destroy(&attr);
  }
}

qbKey keycode_from_sdl(SDL_Keycode sdl_key) {
  switch(sdl_key) {
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
    default: return QB_KEY_UNKNOWN;
  }
}

void send_key_event(qbKey key, bool state) {
  InputEvent input;
  input.was_pressed = key_states[(int)key];
  input.is_pressed = state;
  input.key = key;

  key_states[(int)key] = state;

  qb_event_send(keyboard_event, &input);
}

void send_mouse_click_event(qbButton button, uint8_t is_pressed) {
  MouseEvent event;
  event.event_type = QB_MOUSE_EVENT_BUTTON;
  event.button_event.state = is_pressed;
  event.button_event.mouse_button = button;

  qb_event_send(mouse_event, &event);
}

void send_mouse_move_event(int x, int y, int xrel, int yrel) {
  MouseEvent event;
  event.event_type = QB_MOUSE_EVENT_MOTION;
  event.motion_event.x = x;
  event.motion_event.y = y;
  event.motion_event.xrel = xrel;
  event.motion_event.yrel = yrel;
  mouse_x = x;
  mouse_y = y;

  qb_event_send(mouse_event, &event);
}

qbResult on_key_event(qbSystem system) {
  return qb_event_subscribe(keyboard_event, system);
}

qbResult on_mouse_event(qbSystem system) {
  return qb_event_subscribe(mouse_event, system);
}

bool is_key_pressed(qbKey key) {
  return key_states[(int)key];
}

bool is_mouse_pressed(qbButton mouse_button) {
  return SDL_GetMouseState(nullptr, nullptr) & button_to_sdl(mouse_button);
}

void get_mouse_position(int* x, int* y) {
  SDL_GetMouseState(x, y);
}

uint8_t button_to_sdl(qbButton button) {
  switch (button) {
    case QB_BUTTON_LEFT: return SDL_BUTTON_LEFT;
    case QB_BUTTON_MIDDLE: return SDL_BUTTON_MIDDLE;
    case QB_BUTTON_RIGHT: return SDL_BUTTON_RIGHT;
    case QB_BUTTON_X1: return SDL_BUTTON_X1;
    case QB_BUTTON_X2: return SDL_BUTTON_X2;
  }
  return SDL_BUTTON_LEFT;
}

qbButton button_from_sdl(uint8_t sdl_key) {
  switch (sdl_key) {
    case SDL_BUTTON_LEFT: return QB_BUTTON_LEFT;
    case SDL_BUTTON_MIDDLE: return QB_BUTTON_MIDDLE;
    case SDL_BUTTON_RIGHT: return QB_BUTTON_RIGHT;
    case SDL_BUTTON_X1: return QB_BUTTON_X1;
    case SDL_BUTTON_X2: return QB_BUTTON_X2;
  }
  return QB_BUTTON_LEFT;
}

}  // namespace input

