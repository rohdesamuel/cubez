#include "lua_sprite_bindings.h"
#include <cubez/sprite.h>

#include "lua_common.h"

extern "C" {
#include <lua/lua.h>
#include <lua/lualib.h>
#include <lua/lauxlib.h>
}

int sprite_load(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TSTRING);

  const char* f = lua_tostring(L, 1);

  qbSprite sprite = qb_sprite_load(f);

  lua_pushlightuserdata(L, sprite);
  return 1;
}

int spritesheet_load(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TSTRING);
  LUA_CHECK_TYPE(L, 2, LUA_TNUMBER);
  LUA_CHECK_TYPE(L, 3, LUA_TNUMBER);
  LUA_CHECK_TYPE(L, 4, LUA_TNUMBER);

  const char* f = lua_tostring(L, 1);
  int tw = lua_tointeger(L, 2);
  int th = lua_tointeger(L, 3);
  int margin = lua_tointeger(L, 4);

  qbSprite sprite = qb_spritesheet_load(f, tw, th, margin);

  lua_pushlightuserdata(L, sprite);
  return 1;
}

int sprite_fromsheet(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TLIGHTUSERDATA);
  LUA_CHECK_TYPE(L, 2, LUA_TNUMBER);
  LUA_CHECK_TYPE(L, 3, LUA_TNUMBER);

  qbSprite sprite = (qbSprite)lua_touserdata(L, 1);
  int ix = lua_tointeger(L, 2);
  int iy = lua_tointeger(L, 3);

  lua_pushlightuserdata(L, sprite);

  return 1;
}

int sprite_draw(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TLIGHTUSERDATA);
  LUA_CHECK_TYPE(L, 2, LUA_TNUMBER);
  LUA_CHECK_TYPE(L, 3, LUA_TNUMBER);
  
  qbSprite sprite = (qbSprite)lua_touserdata(L, 1);
  double x = lua_tonumber(L, 2);
  double y = lua_tonumber(L, 3);

  qb_sprite_draw(sprite, { (float)x, (float)y });

  return 0;
}

int sprite_draw_ext(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TLIGHTUSERDATA);
  LUA_CHECK_TYPE(L, 2, LUA_TNUMBER); // x
  LUA_CHECK_TYPE(L, 3, LUA_TNUMBER); // y

  LUA_CHECK_TYPE(L, 4, LUA_TNUMBER); // sx
  LUA_CHECK_TYPE(L, 5, LUA_TNUMBER); // sy

  LUA_CHECK_TYPE(L, 6, LUA_TNUMBER); // rot

  LUA_CHECK_TYPE(L, 7, LUA_TTABLE); // col

  qbSprite sprite = (qbSprite)lua_touserdata(L, 1);
  double x = lua_tonumber(L, 2);
  double y = lua_tonumber(L, 3);

  double sx = lua_tonumber(L, 4);
  double sy = lua_tonumber(L, 5);

  double rot = lua_tonumber(L, 6);

  float rgba[4] = { 1.f, 1.f, 1.f, 1.f };
  int index = 0;
  lua_pushnil(L);  /* first key */
  while (lua_next(L, 7) != 0 && index <= 4) {
    rgba[index] = (float)lua_tonumber(L, -1);

    /* removes 'value'; keeps 'key' for next iteration */
    lua_pop(L, 1);
    ++index;
  }
  vec4s col;
  memcpy(&col, rgba, sizeof(float) * 4);

  qb_sprite_draw_ext(sprite, { (float)x, (float)y }, { (float)sx, (float)sy }, (float)rot, col);

  return 0;
}

int sprite_drawpart(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TLIGHTUSERDATA);
  LUA_CHECK_TYPE(L, 2, LUA_TNUMBER);
  LUA_CHECK_TYPE(L, 3, LUA_TNUMBER);
  LUA_CHECK_TYPE(L, 4, LUA_TNUMBER);
  LUA_CHECK_TYPE(L, 5, LUA_TNUMBER);
  LUA_CHECK_TYPE(L, 6, LUA_TNUMBER);
  LUA_CHECK_TYPE(L, 7, LUA_TNUMBER);

  qbSprite sprite = (qbSprite)lua_touserdata(L, 1);
  double x = lua_tonumber(L, 2);
  double y = lua_tonumber(L, 3);
  int left = lua_tointeger(L, 4);
  int top = lua_tointeger(L, 5);
  int width = lua_tointeger(L, 6);
  int height = lua_tointeger(L, 7);

  qb_sprite_drawpart(sprite, { (float)x, (float)y }, left, top, width, height);

  return 0;
}

int sprite_drawpart_ext(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TLIGHTUSERDATA); // sprite
  LUA_CHECK_TYPE(L, 2, LUA_TNUMBER); // pos x
  LUA_CHECK_TYPE(L, 3, LUA_TNUMBER); // pos y
  LUA_CHECK_TYPE(L, 4, LUA_TNUMBER); // left
  LUA_CHECK_TYPE(L, 5, LUA_TNUMBER); // top
  LUA_CHECK_TYPE(L, 6, LUA_TNUMBER); // width
  LUA_CHECK_TYPE(L, 7, LUA_TNUMBER); // height

  LUA_CHECK_TYPE(L, 8, LUA_TNUMBER); // sx
  LUA_CHECK_TYPE(L, 9, LUA_TNUMBER); // sy
  LUA_CHECK_TYPE(L, 10, LUA_TNUMBER); // rot
  LUA_CHECK_TYPE(L, 11, LUA_TTABLE); // col

  qbSprite sprite = (qbSprite)lua_touserdata(L, 1);
  double x = lua_tonumber(L, 2);
  double y = lua_tonumber(L, 3);
  int left = lua_tointeger(L, 4);
  int top = lua_tointeger(L, 5);
  int width = lua_tointeger(L, 6);
  int height = lua_tointeger(L, 7);

  double sx = lua_tonumber(L, 8);
  double sy = lua_tonumber(L, 9);

  double rot = lua_tonumber(L, 10);

  float rgba[4] = { 1.f, 1.f, 1.f, 1.f };
  int index = 0;
  lua_pushnil(L);  /* first key */
  while (lua_next(L, 11) != 0 && index <= 4) {
    rgba[index] = (float)lua_tonumber(L, -1);

    /* removes 'value'; keeps 'key' for next iteration */
    lua_pop(L, 1);
    ++index;
  }
  vec4s col;
  memcpy(&col, rgba, sizeof(float) * 4);

  qb_sprite_drawpart_ext(sprite, { (float)x, (float)y }, left, top, width, height, { (float)sx, (float)sy }, rot, col);

  return 0;
}

int sprite_getoffset(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TLIGHTUSERDATA);

  qbSprite sprite = (qbSprite)lua_touserdata(L, 1);

  vec2s offset = qb_sprite_getoffset(sprite);
  lua_pushnumber(L, offset.x);
  lua_pushnumber(L, offset.y);

  return 2;
}

int sprite_setoffset(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TLIGHTUSERDATA);
  LUA_CHECK_TYPE(L, 2, LUA_TNUMBER); // x
  LUA_CHECK_TYPE(L, 3, LUA_TNUMBER); // y

  qbSprite sprite = (qbSprite)lua_touserdata(L, 1);
  double x = lua_tointeger(L, 2);
  double y = lua_tointeger(L, 3);

  qb_sprite_setoffset(sprite, { (float)x, (float)y });

  return 0;
}

int sprite_width(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TLIGHTUSERDATA);

  qbSprite sprite = (qbSprite)lua_touserdata(L, 1);

  lua_pushnumber(L, qb_sprite_width(sprite));

  return 1;
}

int sprite_height(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TLIGHTUSERDATA);

  qbSprite sprite = (qbSprite)lua_touserdata(L, 1);

  lua_pushnumber(L, qb_sprite_height(sprite));

  return 1;
}

int sprite_setdepth(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TLIGHTUSERDATA);
  LUA_CHECK_TYPE(L, 2, LUA_TNUMBER);

  qbSprite sprite = (qbSprite)lua_touserdata(L, 1);
  double depth = lua_tonumber(L, 2);

  qb_sprite_setdepth(sprite, (float)depth);

  return 0;
}

int sprite_getdepth(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TLIGHTUSERDATA);

  qbSprite sprite = (qbSprite)lua_touserdata(L, 1);

  lua_pushnumber(L, qb_sprite_getdepth(sprite));

  return 1;
}

int sprite_framecount(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TLIGHTUSERDATA);

  qbSprite sprite = (qbSprite)lua_touserdata(L, 1);

  lua_pushinteger(L, qb_sprite_framecount(sprite));

  return 1;
}

int sprite_getframe(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TLIGHTUSERDATA);

  qbSprite sprite = (qbSprite)lua_touserdata(L, 1);

  lua_pushinteger(L, qb_sprite_getframe(sprite));

  return 1;
}

int sprite_setframe(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TLIGHTUSERDATA);
  LUA_CHECK_TYPE(L, 2, LUA_TNUMBER);

  qbSprite sprite = (qbSprite)lua_touserdata(L, 1);
  uint32_t frame_index = (uint32_t)lua_tointeger(L, 2);

  qb_sprite_setframe(sprite, frame_index);

  return 0;
}

int animation_create(lua_State *L) {
  LUA_CHECK_TYPE(L, 1, LUA_TTABLE); // frames

  LUA_CHECK_TYPE(L, 2, LUA_TNUMBER); // frame_speed
  LUA_CHECK_TYPE(L, 3, LUA_TBOOLEAN); // repeat
  LUA_CHECK_TYPE(L, 4, LUA_TNUMBER); // keyframe

  std::vector<qbSprite> frames;
  lua_pushnil(L);  /* first key */
  while (lua_next(L, 1) != 0) {
    frames.push_back((qbSprite)lua_touserdata(L, -1));
    /* removes 'value'; keeps 'key' for next iteration */
    lua_pop(L, 1);
  }

  double frame_speed = lua_tonumber(L, 2);
  bool repeat = lua_toboolean(L, 3) == 1;
  int64_t keyframe = lua_tointeger(L, 4);

  qbAnimationAttr_ attr{};
  attr.frames = frames.data();
  attr.frame_count = frames.size();
  attr.frame_speed = frame_speed;
  attr.repeat = repeat;
  attr.keyframe = (int)keyframe;

  lua_pushlightuserdata(L, qb_animation_create(&attr));

  return 1;
}

int animation_fromsheet(lua_State* L) {  
  LUA_CHECK_TYPE(L, 1, LUA_TLIGHTUSERDATA); // sprite
  LUA_CHECK_TYPE(L, 2, LUA_TNUMBER); // index_start
  LUA_CHECK_TYPE(L, 3, LUA_TNUMBER); // index_end

  LUA_CHECK_TYPE(L, 4, LUA_TNUMBER); // frame_speed
  LUA_CHECK_TYPE(L, 5, LUA_TBOOLEAN); // repeat
  LUA_CHECK_TYPE(L, 6, LUA_TNUMBER); // keyframe  
  
  qbSprite sheet = (qbSprite)lua_touserdata(L, 1);
  int64_t index_start = lua_tointeger(L, 2);
  int64_t index_end = lua_tointeger(L, 3);

  double frame_speed = lua_tonumber(L, 4);
  bool repeat = lua_toboolean(L, 5) == 1;
  int64_t keyframe = lua_tointeger(L, 6);

  qbAnimationAttr_ attr{};
  attr.frame_speed = frame_speed;
  attr.repeat = repeat;
  attr.keyframe = (int)keyframe;

  lua_pushlightuserdata(L, qb_animation_fromsheet(&attr, sheet, index_start, index_end));

  return 1;
}

int animation_loaddir(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TSTRING); // dir

  LUA_CHECK_TYPE(L, 2, LUA_TNUMBER); // frame_speed
  LUA_CHECK_TYPE(L, 3, LUA_TBOOLEAN); // repeat
  LUA_CHECK_TYPE(L, 4, LUA_TNUMBER); // keyframe

  const char* dir = lua_tostring(L, 1);
  double frame_speed = lua_tonumber(L, 2);
  bool repeat = lua_toboolean(L, 3) == 1;
  int64_t keyframe = lua_tointeger(L, 4);

  qbAnimationAttr_ attr{};
  attr.frame_speed = frame_speed;
  attr.repeat = repeat;
  attr.keyframe = (int)keyframe;

  lua_pushlightuserdata(L, qb_animation_loaddir(dir, &attr));

  return 1;
}

int animation_play(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TLIGHTUSERDATA);

  qbAnimation animation = (qbAnimation)lua_touserdata(L, 1);

  lua_pushlightuserdata(L, qb_animation_play(animation));

  return 1;
}

int animation_getoffset(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TLIGHTUSERDATA);

  qbAnimation animation = (qbAnimation)lua_touserdata(L, 1);

  vec2s offset = qb_animation_getoffset(animation);
  lua_pushnumber(L, offset.x);
  lua_pushnumber(L, offset.y);

  return 2;
}

int animation_setoffset(lua_State* L) {
  LUA_CHECK_TYPE(L, 1, LUA_TLIGHTUSERDATA);
  LUA_CHECK_TYPE(L, 2, LUA_TNUMBER);
  LUA_CHECK_TYPE(L, 3, LUA_TNUMBER);

  qbAnimation animation = (qbAnimation)lua_touserdata(L, 1);
  double ox = lua_tonumber(L, 2);
  double oy = lua_tonumber(L, 3);

  qb_animation_setoffset(animation, { (float)ox, (float)oy });
  return 0;
}