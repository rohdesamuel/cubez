#ifndef INPUT__H
#define INPUT__H

#include "constants.h"
#include "inc/common.h"
#include "inc/cubez.h"
#include <SDL2/SDL_keycode.h>

namespace input {

void initialize();

#define  QB_KEY_RELEASED 0
#define QB_KEY_PRESSED 1

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
  QB_KEY_a,
  QB_KEY_b,
  QB_KEY_c,
  QB_KEY_d,
  QB_KEY_e,
  QB_KEY_f,
  QB_KEY_g,
  QB_KEY_h,
  QB_KEY_i,
  QB_KEY_j,
  QB_KEY_k,
  QB_KEY_l,
  QB_KEY_m,
  QB_KEY_n,
  QB_KEY_o,
  QB_KEY_p,
  QB_KEY_q,
  QB_KEY_r,
  QB_KEY_s,
  QB_KEY_t,
  QB_KEY_u,
  QB_KEY_v,
  QB_KEY_w,
  QB_KEY_x,
  QB_KEY_y,
  QB_KEY_z,
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

struct InputEvent {
  bool was_pressed;
  bool is_pressed;
  qbKey key;
};

void send_key_event(qbKey key, bool is_pressed);
void on_key_event(qbSystem system);
qbKey keycode_from_sdl(SDL_Keycode sdl_key);

}  // namespace input

#endif  // INPUT__H

