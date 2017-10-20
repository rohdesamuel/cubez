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
    case SDLK_w: return QB_KEY_W;
    case SDLK_a: return QB_KEY_A;
    case SDLK_s: return QB_KEY_S;
    case SDLK_d: return QB_KEY_D;
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

void on_key_event(qbSystem system) {
  qb_event_subscribe(input_event, system);
}

bool is_key_pressed(qbKey key) {
  return key_states[(int)key];
}

}  // namespace input

