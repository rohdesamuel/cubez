#include "input.h"
#include <unordered_map>

namespace input {

qbEvent input_event;
std::unordered_map<int, bool> key_states;

void initialize() {
  qbEventAttr attr;
  qb_eventattr_create(&attr);
  qb_eventattr_setmessagetype(attr, InputEvent);
  qb_event_create(&input_event, attr);
  qb_eventattr_destroy(&attr);
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
  input.was_pressed = key_states[key];
  input.is_pressed = state;
  input.key = key;

  key_states[(int)key] = state;

  qb_event_send(input_event, &input);
}

qbResult on_key_event(qbSystem system) {
  return qb_event_subscribe(input_event, system);
}

bool is_key_pressed(qbKey key) {
  return key_states[(int)key];
}

}  // namespace input

