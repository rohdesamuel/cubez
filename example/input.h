#ifndef INPUT__H
#define INPUT__H

#include "constants.h"
#include "inc/common.h"
#include "inc/cubez.h"
#include <SDL2/SDL_keycode.h>

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

namespace input {

void initialize();

#define  QB_KEY_RELEASED 0
#define QB_KEY_PRESSED 1

struct InputEvent {
  bool was_pressed;
  bool is_pressed;
  qbKey key;
};

void send_key_event(qbKey key, bool is_pressed);
void on_key_event(qbSystem system);
bool is_key_pressed(qbKey key);
qbKey keycode_from_sdl(SDL_Keycode sdl_key);

}  // namespace input

#endif  // INPUT__H

