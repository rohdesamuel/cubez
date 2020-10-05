#include "lua_input_bindings.h"

#include <unordered_map>
#include <cubez/input.h>

extern "C" {
#include <lua/lua.h>
#include <lua/lualib.h>
#include <lua/lauxlib.h>
}

std::unordered_map<std::string, qbKey> lua_to_qb_key;
std::unordered_map<std::string, qbScanCode> lua_to_qb_scan;
std::unordered_map<std::string, qbButton> lua_to_qb_mouse;

void lua_input_initialize() {
  lua_to_qb_key = {
    { "return", QB_KEY_RETURN },
    { "escape", QB_KEY_ESCAPE },
    { "backspace", QB_KEY_BACKSPACE },
    { "tab", QB_KEY_TAB },
    { "space", QB_KEY_SPACE },
    { "!", QB_KEY_EXCLAIM },
    { "\"", QB_KEY_QUOTEDBL },

    { "#", QB_KEY_HASH },
    { "%", QB_KEY_PERCENT },
    { "$", QB_KEY_DOLLAR },
    { "&", QB_KEY_AMPERSAND },
    { "'", QB_KEY_QUOTE },
    { "(", QB_KEY_LPAREN },
    { ")", QB_KEY_RPAREN },
    { "*", QB_KEY_ASTERISK },
    { "+", QB_KEY_PLUS },
    { ",", QB_KEY_COMMA },
    { "-", QB_KEY_MINUS },
    { ".", QB_KEY_PERIOD },
    { "/", QB_KEY_SLASH },
    { "0", QB_KEY_0 },
    { "1", QB_KEY_1 },
    { "2", QB_KEY_2 },
    { "3", QB_KEY_3 },
    { "4", QB_KEY_4 },
    { "5", QB_KEY_5 },
    { "6", QB_KEY_6 },
    { "7", QB_KEY_7 },
    { "8", QB_KEY_8 },
    { "9", QB_KEY_9 },
    { ":", QB_KEY_COLON },
    { ";", QB_KEY_SEMICOLON },
    { "<", QB_KEY_LESS },
    { "=", QB_KEY_EQUALS },
    { ">", QB_KEY_GREATER },
    { "?", QB_KEY_QUESTION },
    { "@", QB_KEY_AT },
    { "[", QB_KEY_LBRACKET },
    { "\\", QB_KEY_BACKSLASH },
    { "]", QB_KEY_RBRACKET },
    { "^", QB_KEY_CARET },
    { "_", QB_KEY_UNDERSCORE },
    { "`", QB_KEY_BACKQUOTE },
    { "a", QB_KEY_A },
    { "b", QB_KEY_B },
    { "c", QB_KEY_C },
    { "d", QB_KEY_D },
    { "e", QB_KEY_E },
    { "f", QB_KEY_F },
    { "g", QB_KEY_G },
    { "h", QB_KEY_H },
    { "i", QB_KEY_I },
    { "j", QB_KEY_J },
    { "k", QB_KEY_K },
    { "l", QB_KEY_L },
    { "m", QB_KEY_M },
    { "n", QB_KEY_N },
    { "o", QB_KEY_O },
    { "p", QB_KEY_P },
    { "q", QB_KEY_Q },
    { "r", QB_KEY_R },
    { "s", QB_KEY_S },
    { "t", QB_KEY_T },
    { "u", QB_KEY_U },
    { "v", QB_KEY_V },
    { "w", QB_KEY_W },
    { "x", QB_KEY_X },
    { "y", QB_KEY_Y },
    { "z", QB_KEY_Z },

    { "f1", QB_KEY_F1 },
    { "f2", QB_KEY_F2 },
    { "f3", QB_KEY_F3 },
    { "f4", QB_KEY_F4 },
    { "f5", QB_KEY_F5 },
    { "f6", QB_KEY_F6 },
    { "f7", QB_KEY_F7 },
    { "f8", QB_KEY_F8 },
    { "f9", QB_KEY_F9 },
    { "f10", QB_KEY_F10 },
    { "f11", QB_KEY_F11 },
    { "f12", QB_KEY_F12 },

    { "home", QB_KEY_HOME },
    { "end", QB_KEY_END },
    { "pageup", QB_KEY_PAGEUP },
    { "pagedown", QB_KEY_PAGEDOWN },
    { "delete", QB_KEY_DELETE },
    { "insert", QB_KEY_INSERT },

    { "up", QB_KEY_UP },
    { "down", QB_KEY_DOWN },
    { "left", QB_KEY_LEFT },
    { "right", QB_KEY_RIGHT },

    { "kp=", QB_KEY_KP_EQUALS },
    { "kp/", QB_KEY_KP_DIVIDE },
    { "kp*", QB_KEY_KP_MULTIPLY },
    { "kp-", QB_KEY_KP_MINUS },
    { "kp+", QB_KEY_KP_PLUS },
    { "kpenter", QB_KEY_KP_ENTER },
    { "kp1", QB_KEY_KP_1 },
    { "kp2", QB_KEY_KP_2 },
    { "kp3", QB_KEY_KP_3 },
    { "kp4", QB_KEY_KP_4 },
    { "kp5", QB_KEY_KP_5 },
    { "kp6", QB_KEY_KP_6 },
    { "kp7", QB_KEY_KP_7 },
    { "kp8", QB_KEY_KP_8 },
    { "kp9", QB_KEY_KP_9 },
    { "kp0", QB_KEY_KP_0 },
    { "kp.", QB_KEY_KP_PERIOD },
  };
  lua_to_qb_scan = {
    { "return", QB_SCANCODE_RETURN },
    { "escape", QB_SCANCODE_ESCAPE },
    { "backspace", QB_SCANCODE_BACKSPACE },
    { "tab", QB_SCANCODE_TAB },
    { "space", QB_SCANCODE_SPACE },

    { ",", QB_SCANCODE_COMMA },
    { "-", QB_SCANCODE_MINUS },
    { ".", QB_SCANCODE_PERIOD },
    { "/", QB_SCANCODE_SLASH },
    { "0", QB_SCANCODE_0 },
    { "1", QB_SCANCODE_1 },
    { "2", QB_SCANCODE_2 },
    { "3", QB_SCANCODE_3 },
    { "4", QB_SCANCODE_4 },
    { "5", QB_SCANCODE_5 },
    { "6", QB_SCANCODE_6 },
    { "7", QB_SCANCODE_7 },
    { "8", QB_SCANCODE_8 },
    { "9", QB_SCANCODE_9 },
    { ";", QB_SCANCODE_SEMICOLON },
    { "=", QB_SCANCODE_EQUALS },
    { "[", QB_SCANCODE_LEFTBRACKET },
    { "\\", QB_SCANCODE_BACKSLASH },
    { "]", QB_SCANCODE_RIGHTBRACKET },
    { "`", QB_SCANCODE_GRAVE },
    { "a", QB_SCANCODE_A },
    { "b", QB_SCANCODE_B },
    { "c", QB_SCANCODE_C },
    { "d", QB_SCANCODE_D },
    { "e", QB_SCANCODE_E },
    { "f", QB_SCANCODE_F },
    { "g", QB_SCANCODE_G },
    { "h", QB_SCANCODE_H },
    { "i", QB_SCANCODE_I },
    { "j", QB_SCANCODE_J },
    { "k", QB_SCANCODE_K },
    { "l", QB_SCANCODE_L },
    { "m", QB_SCANCODE_M },
    { "n", QB_SCANCODE_N },
    { "o", QB_SCANCODE_O },
    { "p", QB_SCANCODE_P },
    { "q", QB_SCANCODE_Q },
    { "r", QB_SCANCODE_R },
    { "s", QB_SCANCODE_S },
    { "t", QB_SCANCODE_T },
    { "u", QB_SCANCODE_U },
    { "v", QB_SCANCODE_V },
    { "w", QB_SCANCODE_W },
    { "x", QB_SCANCODE_X },
    { "y", QB_SCANCODE_Y },
    { "z", QB_SCANCODE_Z },

    { "f1", QB_SCANCODE_F1 },
    { "f2", QB_SCANCODE_F2 },
    { "f3", QB_SCANCODE_F3 },
    { "f4", QB_SCANCODE_F4 },
    { "f5", QB_SCANCODE_F5 },
    { "f6", QB_SCANCODE_F6 },
    { "f7", QB_SCANCODE_F7 },
    { "f8", QB_SCANCODE_F8 },
    { "f9", QB_SCANCODE_F9 },
    { "f10", QB_SCANCODE_F10 },
    { "f11", QB_SCANCODE_F11 },
    { "f12", QB_SCANCODE_F12 },

    { "home", QB_SCANCODE_HOME },
    { "end", QB_SCANCODE_END },
    { "pageup", QB_SCANCODE_PAGEUP },
    { "pagedown", QB_SCANCODE_PAGEDOWN },
    { "delete", QB_SCANCODE_DELETE },
    { "insert", QB_SCANCODE_INSERT },

    { "up", QB_SCANCODE_UP },
    { "down", QB_SCANCODE_DOWN },
    { "left", QB_SCANCODE_LEFT },
    { "right", QB_SCANCODE_RIGHT },

    { "kp=", QB_SCANCODE_KP_EQUALS },
    { "kp/", QB_SCANCODE_KP_DIVIDE },
    { "kp*", QB_SCANCODE_KP_MULTIPLY },
    { "kp-", QB_SCANCODE_KP_MINUS },
    { "kp+", QB_SCANCODE_KP_PLUS },
    { "kpenter", QB_SCANCODE_KP_ENTER },
    { "kp1", QB_SCANCODE_KP_1 },
    { "kp2", QB_SCANCODE_KP_2 },
    { "kp3", QB_SCANCODE_KP_3 },
    { "kp4", QB_SCANCODE_KP_4 },
    { "kp5", QB_SCANCODE_KP_5 },
    { "kp6", QB_SCANCODE_KP_6 },
    { "kp7", QB_SCANCODE_KP_7 },
    { "kp8", QB_SCANCODE_KP_8 },
    { "kp9", QB_SCANCODE_KP_9 },
    { "kp0", QB_SCANCODE_KP_0 },
    { "kp.", QB_SCANCODE_KP_PERIOD },
  };
  lua_to_qb_mouse = {
    { "left", QB_BUTTON_LEFT },
    { "middle", QB_BUTTON_MIDDLE },
    { "right", QB_BUTTON_RIGHT },
    { "x1", QB_BUTTON_X1 },
    { "x2", QB_BUTTON_X2 },
  };
}

int key_pressed(lua_State* L) {
  const char* key = lua_tostring(L, 1);
  auto it = lua_to_qb_key.find(key);
  if (it != lua_to_qb_key.end()) {
    lua_pushboolean(L, qb_key_ispressed(it->second) ? 1 : 0);
  } else {
    lua_pushboolean(L, 0);
  }

  return 1;
}

int scan_pressed(lua_State* L) {
  const char* scan_code = lua_tostring(L, 1);
  auto it = lua_to_qb_scan.find(scan_code);
  if (it != lua_to_qb_scan.end()) {
    lua_pushboolean(L, qb_scancode_ispressed(it->second) ? 1 : 0);
  } else {
    lua_pushboolean(L, 0);
  }

  return 1;
}

int mouse_pressed(lua_State* L) {
  const char* mouse_button = lua_tostring(L, 1);
  auto it = lua_to_qb_mouse.find(mouse_button);
  if (it != lua_to_qb_mouse.end()) {
    lua_pushboolean(L, qb_mouse_ispressed(it->second) ? 1 : 0);
  } else {
    lua_pushboolean(L, 0);
  }

  return 1;
}

int mouse_position(lua_State* L) {
  int x, y;
  qb_mouse_getposition(&x, &y);
  lua_pushinteger(L, x);
  lua_pushinteger(L, y);
  return 2;
}

int mouse_wheel(lua_State* L) {
  int x, y;
  qb_mouse_getwheel(&x, &y);
  lua_pushinteger(L, x);
  lua_pushinteger(L, y);
  return 2;
}

int mouse_setrelative(lua_State* L) {
  qb_mouse_setrelative(lua_tointeger(L, 1));
  return 0;
}

int mouse_relative(lua_State* L) {
  lua_pushboolean(L, qb_mouse_getrelative());
  return 1;
}

int userfocus(lua_State* L) {
  auto focus = qb_user_focus();

  switch (focus) {
    case qbInputFocus::QB_FOCUS_APP:
      lua_pushstring(L, "app");
      break;

    case qbInputFocus::QB_FOCUS_GUI:
      lua_pushstring(L, "gui");
      break;
  }

  return 1;
}