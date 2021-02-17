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
  qbAudioPlaying aud = qb_audio_upload(buf);
  
  lua_pushlightuserdata(L, aud);
  return 1;
}

int audio_free(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TLIGHTUSERDATA);

  qbAudioPlaying aud = (qbAudioPlaying)lua_touserdata(L, 1);
  qb_audio_free(qb_audio_buffer(aud));

  return 0;
}

int audio_isplaying(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TLIGHTUSERDATA);

  qbAudioPlaying aud = (qbAudioPlaying)lua_touserdata(L, 1);
  lua_pushboolean(L, qb_audio_isplaying(aud));

  return 1;
}

int audio_play(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TLIGHTUSERDATA);

  qbAudioPlaying aud = (qbAudioPlaying)lua_touserdata(L, 1);
  qb_audio_play(aud);
  
  return 0;
}

int audio_stop(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TLIGHTUSERDATA);

  qbAudioPlaying aud = (qbAudioPlaying)lua_touserdata(L, 1);
  qb_audio_stop(aud);

  return 0;
}

int audio_pause(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TLIGHTUSERDATA);

  qbAudioPlaying aud = (qbAudioPlaying)lua_touserdata(L, 1);
  qb_audio_pause(aud);

  return 0;
}

int audio_setloop(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TLIGHTUSERDATA);
  LUA_CHECK_TYPE(L, 2, LUA_TBOOLEAN);

  qbAudioPlaying aud = (qbAudioPlaying)lua_touserdata(L, 1);
  int should_loop = lua_toboolean(L, 2);
  qb_audio_loop(aud, should_loop == 0 ? QB_AUDIO_LOOP_DISABLE : QB_AUDIO_LOOP_ENABLE);

  return 0;
}

int audio_setpan(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TLIGHTUSERDATA);
  LUA_CHECK_TYPE(L, 2, LUA_TNUMBER);  

  qbAudioPlaying aud = (qbAudioPlaying)lua_touserdata(L, 1);
  double pan = lua_tonumber(L, 2);

  qb_audio_setpan(aud, pan);

  return 0;
}

int audio_setvolume(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TLIGHTUSERDATA);
  LUA_CHECK_TYPE(L, 2, LUA_TNUMBER);

  qbAudioPlaying aud = (qbAudioPlaying)lua_touserdata(L, 1);
  double vol = lua_tonumber(L, 2);

  qb_audio_setvolume(aud, vol, vol);

  return 0;
}

int audio_stopall(lua_State* L) {
  qb_audio_stopall();
  return 0;
}