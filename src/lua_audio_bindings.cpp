#include "lua_audio_bindings.h"
#include "lua_common.h"

#include <cubez/audio.h>

extern "C" {
#include <lua/lua.h>
#include <lua/lualib.h>
#include <lua/lauxlib.h>
}

int audio_loadwav(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TSTRING);

  const char* f = lua_tostring(L, 1);

  qbAudioBuffer buf = qb_audio_loadwav(f);
  
  lua_pushlightuserdata(L, buf);
  return 1;
}

int audio_free(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TLIGHTUSERDATA);

  qbAudioBuffer aud = (qbAudioBuffer)lua_touserdata(L, 1);
  qb_audio_free(aud);

  return 0;
}

int audio_play(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TLIGHTUSERDATA);

  qbAudioBuffer buf = (qbAudioBuffer)lua_touserdata(L, 1);
  qbHandle ret = qb_audio_play(buf);
  
  lua_pushinteger(L, ret);
  return 1;
}

int audio_stop(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TNUMBER);

  qbId handle = lua_tointeger(L, 1);

  qb_audio_stop(handle);

  return 0;
}

int audio_pause(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TNUMBER);

  qbId handle = lua_tointeger(L, 1);
  qb_audio_pause(handle);

  return 0;
}

int audio_setloop(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TNUMBER);
  LUA_CHECK_TYPE(L, 2, LUA_TBOOLEAN);

  qbId handle = lua_tointeger(L, 1);
  int should_loop = lua_toboolean(L, 2);
  qb_audio_loop(handle, should_loop);

  return 0;
}

int audio_setpan(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TNUMBER);
  LUA_CHECK_TYPE(L, 2, LUA_TNUMBER);  

  qbId handle = lua_tointeger(L, 1);
  double pan = lua_tonumber(L, 2);

  qb_audio_setpan(handle, (float)pan);

  return 0;
}

int audio_setvolume(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TNUMBER);
  LUA_CHECK_TYPE(L, 2, LUA_TNUMBER);

  int num_args = lua_gettop(L);
  if (num_args == 2 || lua_isnil(L, 3)) {
    qbId handle = lua_tointeger(L, 1);
    double left = lua_tonumber(L, 2);

    qb_audio_setvolume(handle, (float)left, (float)left);
  } else if (num_args == 3) {
    qbId handle = lua_tointeger(L, 1);
    double left = lua_tonumber(L, 2);
    double right = lua_tonumber(L, 3);

    qb_audio_setvolume(handle, (float)left, (float)right);
  }

  return 0;
}

int audio_isplaying(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TNUMBER);

  qbId handle = lua_tointeger(L, 1);
  lua_pushboolean(L, qb_audio_isplaying(handle));

  return 1;
}

int audio_stopall(lua_State* L) {
  qb_audio_stopall();
  return 0;
}