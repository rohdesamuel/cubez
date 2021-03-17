#include "lua_draw_bindings.h"
#include "lua_common.h"
#include <cubez/render.h>

extern "C" {
#include <lua/lua.h>
#include <lua/lualib.h>
#include <lua/lauxlib.h>
}

int window_width(lua_State* L) {
  lua_pushinteger(L, qb_window_width());
  return 1;
}

int window_height(lua_State* L) {
  lua_pushinteger(L, qb_window_height());
  return 1;
}

int window_getfullscreen(lua_State* L) {
  auto fs = qb_window_fullscreen();
  switch (fs) {
    case QB_WINDOWED:
      lua_pushstring(L, "windowed");
      break;
    case QB_WINDOW_FULLSCREEN:
      lua_pushstring(L, "window_fullscreen");
      break;
    case QB_WINDOW_FULLSCREEN_DESKTOP:
      lua_pushstring(L, "window_fullscreen_desktop");
      break;
  }
  return 1;
}

int window_setfullscreen(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TSTRING);

  const char* s = lua_tostring(L, 1);
  if (strcmp(s, "windowed") == 0) {
    qb_window_setfullscreen(QB_WINDOWED);
  } else if (strcmp(s, "window_fullscreen") == 0) {
    qb_window_setfullscreen(QB_WINDOW_FULLSCREEN);
  } else if (strcmp(s, "window_fullscreen_desktop") == 0) {
    qb_window_setfullscreen(QB_WINDOW_FULLSCREEN_DESKTOP);
  }

  return 0;
}

int window_getbordered(lua_State* L) {
  lua_pushboolean(L, qb_window_bordered());

  return 1;
}

int window_setbordered(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TBOOLEAN);

  qb_window_setbordered(lua_toboolean(L, 1));

  return 0;
}