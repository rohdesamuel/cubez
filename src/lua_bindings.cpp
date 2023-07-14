/**
* Author: Samuel Rohde (rohde.samuel@cubez.io)
*
* Copyright 2020 Samuel Rohde
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include <cubez/common.h>
#include <cubez/cubez.h>
#include <cubez/render.h>
#include "component_registry.h"
#include "lua_bindings.h"
#include "lua_common.h"

#include "lua_cubez_bindings.h"
#include "lua_input_bindings.h"
#include "lua_gui_bindings.h"
#include "lua_audio_bindings.h"
#include "lua_sprite_bindings.h"
#include "lua_draw_bindings.h"

extern "C" {
#include <lua/lua.h>
#include <lua/lualib.h>
#include <lua/lauxlib.h>
}

#include <string>
#include <iostream>
#include <vector>
#include <tuple>
#include <functional>
#include <algorithm>

#include <filesystem>

static const luaL_Reg qb_lib[] = {
  { "component_create", component_create },
  { "component_count", component_count },
  { "component_find", component_find },

  { "entity_create", entity_create },
  { "entity_destroy", entity_destroy },
  { "entity_addcomponent", entity_addcomponent },
  { "entity_removecomponent", entity_removecomponent },
  { "entity_hascomponent", entity_hascomponent },
  { "entity_getcomponent", entity_getcomponent },

  { "instance_get", instance_get },
  { "instance_set", instance_set },

  { "system_create", system_create },

  { "map_get", map_get },
  { "map_set", map_set },
  { "array_get", array_get },
  { "array_set", array_set },

  { "key_ispressed", key_pressed },
  { "scan_ispressed", scan_pressed },
  { "mouse_ispressed", mouse_pressed },
  { "mouse_getposition" , mouse_position},
  { "mouse_getwheel", mouse_wheel},
  { "mouse_setrelative", mouse_setrelative },
  { "mouse_relative", mouse_relative },
  { "userfocus", userfocus },

  { "window_width", window_width },
  { "window_height", window_height },
  { "window_getfullscreen", window_getfullscreen },
  { "window_setfullscreen", window_setfullscreen },
  { "window_getbordered", window_getbordered },
  { "window_setbordered", window_setbordered },

  { "guielement_getfocus", guielement_getfocus },
  { "guielement_getfocusat", guielement_getfocusat },
  { "guielement_find", guielement_find },
  { "guielement_create", guielement_create },
  { "guielement_destroy", guielement_destroy },
  { "guielement_open", guielement_open },
  { "guielement_close", guielement_close },
  { "guielement_closechildren", guielement_closechildren },
  { "guielement_getconstraint", guielement_getconstraint },
  { "guielement_setconstraint", guielement_setconstraint },  
  { "guielement_clearconstraint", guielement_clearconstraint },
  { "guielement_link", guielement_link },
  { "guielement_unlink", guielement_unlink },
  { "guielement_getparent", guielement_getparent },
  { "guielement_movetofront", guielement_movetofront },
  { "guielement_movetoback", guielement_movetoback },
  { "guielement_moveforward", guielement_moveforward },
  { "guielement_movebackward", guielement_movebackward },
  { "guielement_moveto", guielement_moveto },
  { "guielement_moveby", guielement_moveby },
  { "guielement_resizeto", guielement_resizeto },
  { "guielement_resizeby", guielement_resizeby },
  { "guielement_getsize", guielement_getsize },
  { "guielement_getposition", guielement_getposition },
  { "guielement_setvalue", guielement_setvalue },
  { "guielement_getvalue", guielement_getvalue },
  { "guielement_gettext", guielement_gettext },
  { "guielement_settext", guielement_settext },
  { "guielement_settextcolor", guielement_settextcolor },
  { "guielement_settextscale", guielement_settextscale },
  { "guielement_settextsize", guielement_settextsize },

  { "audio_loadwav", audio_loadwav },
  { "audio_free", audio_free },
  { "audio_isplaying", audio_isplaying },
  { "audio_play", audio_play },
  { "audio_stop", audio_stop },
  { "audio_pause", audio_pause },
  { "audio_setloop", audio_setloop },
  { "audio_setpan", audio_setpan },
  { "audio_setvolume", audio_setvolume },
  { "audio_stopall", audio_stopall },

  { "sprite_load", sprite_load },
  { "spritesheet_load", spritesheet_load },
  { "sprite_fromsheet", sprite_fromsheet },
  { "sprite_draw", sprite_draw },
  { "sprite_draw_ext", sprite_draw_ext },
  { "sprite_drawpart", sprite_drawpart },
  { "sprite_drawpart_ext", sprite_drawpart_ext },
  { "sprite_getoffset", sprite_getoffset },
  { "sprite_setoffset", sprite_setoffset },
  { "sprite_width", sprite_width },
  { "sprite_height", sprite_height },
  { "sprite_setdepth", sprite_setdepth },
  { "sprite_getdepth", sprite_getdepth },

  { "sprite_framecount", sprite_framecount },
  { "sprite_getframe", sprite_getframe },
  { "sprite_setframe", sprite_setframe },

  { "animation_create", animation_create },
  { "animation_loaddir", animation_loaddir },
  { "animation_fromsheet", animation_fromsheet },
  { "animation_play", animation_play },
  { "animation_getoffset", animation_getoffset },
  { "animation_setoffset", animation_setoffset },

  { nullptr, nullptr }
};

const char QB_LUA_LIB[] = "qb";
const char QB_LUA_INIT[] =
R"(

-- Initialization code for the Lua portion of scripting. This is so that a file
-- doesn't have to be included with the game on how to initialize the engine.

local _QB = qb

---------------------------
-- The Component metatable.
---------------------------
_QB.component = {}
_QB.Component = {}
_QB.Component.__index = _QB.Component

function _QB.component.create (name, schema)
  return _QB.component_create(name, schema)
end

function _QB.component.find (name)
  return { id=component_find(name) }
end

function _QB.Component:create (tdata)
  return { id=self.id, _data=tdata }
end

function _QB.Component:count ()
  return _QB.component_count(self.id)
end

--------------------------
-- The Instance metatable.
--------------------------
_QB.Instance = {}
_QB.Instance.__index = _QB.Instance

function qb.Instance:__index (k)
  return _QB.instance_get(self, k)
end

function qb.Instance:__newindex (k, v)
  _QB.instance_set(self, k, v)
end

----------------------------
-- The Map/Array metatables.
----------------------------
_QB.Array = {}
_QB.Array.__index = _QB.Array

function qb.Array:__index (k)
  return _QB.array_get(self, k)
end

function qb.Array:__newindex (k, v)
  _QB.array_set(self, k, v)
end

_QB.Map = {}
_QB.Map.__index = _QB.Map

function qb.Map:__index (k)
  return _QB.map_get(self, k)
end

function qb.Map:__newindex (k, v)
  _QB.map_set(self, k, v)
end

------------------------
-- The Entity metatable.
------------------------
_QB.entity = {}
_QB.Entity = {}
_QB.Entity.__index = _QB.Entity

function _QB.entity.create (...)
  local t = {}
  setmetatable(t, _QB.Entity)
  t.id = _QB.entity_create(...)
  return t
end

function _QB.entity.destroy (entity)
  _QB.entity_destroy(entity.id)
end

function _QB.Entity:add (...)
  _QB.entity_addcomponent(self.id, ...)
end

function _QB.Entity:remove (component)
  _QB.entity_removecomponent(self.id, component.id)
end

function _QB.Entity:has (component)
  return _QB.entity_hascomponent(self.id, component.id)
end

function _QB.Entity:get (component)
  return _QB.entity_getcomponent(self.id, component.id)
end

------------------------
-- System Methods.
------------------------
_QB.system = {}

function _QB.system.create (components, fn)
  return _QB.system_create(components, fn)
end


-----------------
-- Input Methods.
-----------------
_QB.keyboard = {}
_QB.mouse = {}

function _QB.keyboard.ispressed (key)
  return _QB.key_ispressed(key)
end

function _QB.keyboard.isscancodepressed (key)
  return _QB.scan_ispressed(key)
end

function _QB.mouse.ispressed (button)
  return _QB.mouse_ispressed(button)
end

function _QB.mouse.getposition ()
  return _QB.mouse_getposition()
end

function _QB.mouse.getwheel ()
  return _QB.mouse_getwheel()
end

function _QB.mouse.setrelative (relative)
  _QB.mouse_setrelative(relative)
end

function _QB.mouse.getrelative ()
  return _QB.mouse_getrelative()
end

function _QB.userfocus ()
  return _QB.input_focus()
end


-----------------
-- Input Methods.
-----------------
_QB.gui = {}
_QB.gui.property = {
  X='x',
  Y='y',
  WIDTH='width',
  HEIGHT='height'
}

_QB.gui.constraint = {
  PIXEL='pixel',
  PERCENT='percent',
  RELATIVE='relative',
  MIN='min',
  MAX='max',
  ASPECT_RATIO='aspectratio'
}

_QB.gui.align = {
  LEFT='left',
  CENTER='center',
  RIGHT='right'
}

_QB.gui.element = {}
_QB.gui.Element = {}
_QB.gui.Element.__index = _QB.gui.Element

function _QB.gui.getfocus ()
  return _QB.guielement_getfocus()
end

function _QB.gui.getfocusat (pos)
  return _QB.guielement_getfocusat(pos)
end

function _QB.gui.element.find (id)
  return _QB.guielement_find(id)
end

function _QB.gui.element.create (id, options)
  return _QB.guielement_create(id, options)
end

function _QB.gui.element.destroy (element)
  return _QB.guielement_destroy(element._element)
end

function _QB.gui.Element:open ()
  _QB.guielement_open(self._element)
end

function _QB.gui.Element:close ()
  _QB.guielement_close(self._element)
end

function _QB.gui.Element:closechildren ()
  _QB.guielement_closechildren(self._element)
end

function _QB.gui.Element:setconstraint (...)
  _QB.guielement_setconstraint(self._element, ...)
end

function _QB.gui.Element:getconstraint (property, opt_constraint)
  _QB.guielement_getconstraint(self._element, property, opt_constraint)
end

function _QB.gui.Element:clearconstraint (...)
  _QB.guielement_clearconstraint(self._element, ...)
end

function _QB.gui.Element:link (parent)
  _QB.guielement_link(self._element, parent)
end

function _QB.gui.Element:unlink ()
  _QB.guielement_unlink(self._element)
end

function _QB.gui.Element:getparent ()
  return _QB.guielement_getparent(self._element)
end

function _QB.gui.Element:movetofront ()
  _QB.guielement_movetofront(self._element)
end

function _QB.gui.Element:movetoback ()
  _QB.guielement_movetoback(self._element)
end

function _QB.gui.Element:moveforward ()
  _QB.guielement_moveforward(self._element)
end

function _QB.gui.Element:movebackward ()
  _QB.guielement_movebackward(self._element)
end

function _QB.gui.Element:moveto (x, y)
  _QB.guielement_moveto(self._element, x, y)
end

function _QB.gui.Element:moveby (x, y)
  _QB.guielement_moveby(self._element, x, y)
end

function _QB.gui.Element:resizeto (x, y)
  _QB.guielement_resizeto(self._element, x, y)
end

function _QB.gui.Element:resizeby (x, y)
  _QB.guielement_resizeby(self._element, x, y)
end

function _QB.gui.Element:getsize ()
  return _QB.guielement_getsize(self._element)
end

function _QB.gui.Element:getposition ()
  return _QB.guielement_getposition(self._element)
end

function _QB.gui.Element:setvalue (value)
  _QB.guielement_setvalue(self._element, value)
end

function _QB.gui.Element:getvalue ()
  return _QB.guielement_getvalue(self._element)
end

function _QB.gui.Element:settext (value)
  _QB.guielement_settext(self._element, value)
end

function _QB.gui.Element:gettext ()
  return _QB.guielement_gettext(self._element)
end

function _QB.gui.Element:settextcolor (color)
  _QB.guielement_settextcolor(self._element, color)
end

function _QB.gui.Element:settextscale (scale)
  _QB.guielement_settextscale(self._element, scale)
end

function _QB.gui.Element:settextsize (size)
  _QB.guielement_settextsize(self._element, size)
end

function _QB.gui.Element:settextalign (align)
  _QB.guielement_settextalign(self._element, align)
end

-----------------
-- Audio Methods.
-----------------
_QB.audio = {}

_QB.audio.Sample = {}
_QB.audio.Sample.__index = _QB.audio.Sample

_QB.audio.Sound = {}
_QB.audio.Sound.__index = _QB.audio.Sound

function _QB.audio.loadwav (file)
  local ret = { _sample=_QB.audio_loadwav(file) }
  setmetatable(ret, _QB.audio.Sample)

  return ret
end

function _QB.audio.stopall ()
  return _QB.audio_stopall()
end

function _QB.audio.Sample:play ()
  local ret = { _sound=_QB.audio_play(self._sample) }
  setmetatable(ret, _QB.audio.Sound)

  return ret
end

function _QB.audio.Sound:free ()
  return _QB.audio_free(self._sound)
end

function _QB.audio.Sound:isplaying ()
  return _QB.audio_isplaying(self._sound)
end

function _QB.audio.Sound:stop ()
  return _QB.audio_stop(self._sound)
end

function _QB.audio.Sound:pause ()
  return _QB.audio_pause(self._sound)
end

function _QB.audio.Sound:setloop (loop)
  return _QB.audio_setloop(self._sound, loop)
end

function _QB.audio.Sound:setpan (pan)
  return _QB.audio_setpan(self._sound, pan)
end

function _QB.audio.Sound:setvolume (left, right)
  return _QB.audio_setvolume(self._sound, left, right)
end

------------------
-- Draw Methods.
------------------
_QB.sprite = {}

_QB.sprite.Sprite = {}
function _QB.sprite.Sprite.__eq(a, b)
  return a._sprite == b._sprite
end
_QB.sprite.Sprite.__index = _QB.sprite.Sprite

function _QB.sprite.load (file)
  local ret = { _sprite=_QB.sprite_load(file) }
  setmetatable(ret, _QB.sprite.Sprite)

  return ret
end

function _QB.sprite.loadsheet (file, tw, th, margin)
  local ret = { _sprite=_QB.spritesheet_load(file, tw, th, margin) }
  setmetatable(ret, _QB.sprite.Sprite)

  return ret
end

function _QB.sprite.fromsheet (sheet, ix, iy)
  local ret = { _sprite=_QB.sprite_fromsheet(sheet, ix, iy) }
  setmetatable(ret, _QB.sprite.Sprite)

  return ret
end

function _QB.sprite.Sprite:draw (x, y)
  _QB.sprite_draw(self._sprite, x, y)
end

function _QB.sprite.Sprite:draw_ext (x, y, sx, sy, rot, col)
  _QB.sprite_draw_ext(self._sprite, x, y, sx, sy, rot, col)
end

function _QB.sprite.Sprite:drawpart (x, y, left, top, width, height)
  _QB.sprite_drawpart(self._sprite, x, y, left, top, width, height)
end

function _QB.sprite.Sprite:drawpart_ext (x, y, left, top, width, height, sx, sy, rot, col)
  _QB.sprite_drawpart_ext(self._sprite, x, y, left, top, width, height, sx, sy, rot, col)
end

function _QB.sprite.Sprite:getoffset ()
  return _QB.sprite_getoffset(self._sprite)
end

function _QB.sprite.Sprite:setoffset (ox, oy)
  _QB.sprite_setoffset(self._sprite, ox, oy)
end

function _QB.sprite.Sprite:width ()
  return _QB.sprite_width(self._sprite)
end

function _QB.sprite.Sprite:height ()
  return _QB.sprite_height(self._sprite)
end

function _QB.sprite.Sprite:getdepth ()
  return _QB.sprite_getdepth(self._sprite)
end

function _QB.sprite.Sprite:setdepth (depth)
  _QB.sprite_setdepth(self._sprite, depth)
end

function _QB.sprite.Sprite:framecount ()
  return _QB.sprite_framecount(self._sprite)
end

function _QB.sprite.Sprite:getframe()
  return _QB.sprite_getframe(self._sprite)
end

function _QB.sprite.Sprite:setframe(frame)
  _QB.sprite_setframe(self._sprite, frame)
end


_QB.animation = {}
_QB.animation.Animation = {}
function _QB.animation.Animation.__eq(a, b)
  return a._animation == b._animation
end

_QB.animation.Animation.__index = _QB.animation.Animation

function _QB.animation.create (frames, frame_speed, frame_repeat, keyframe)
  local sprites = {}
  for i, sprite in ipairs(frames) do
    sprites[i] = sprite._sprite
  end

  local frame_speed = frame_speed or 1.0
  local frame_repeat = frame_repeat or false
  local keyframe = keyframe or 0

  local ret = { _animation=_QB.animation_create(sprites, frame_speed, frame_repeat, keyframe) }
  setmetatable(ret, _QB.animation.Animation)
  return ret
end

function _QB.animation.loaddir (dir, frame_speed, frame_repeat, keyframe)
  local frame_speed = frame_speed or 1.0
  local frame_repeat = frame_repeat or false
  local keyframe = keyframe or 0

  local ret = { _animation=_QB.animation_loaddir(dir, frame_speed, frame_repeat, keyframe) }
  setmetatable(ret, _QB.animation.Animation)
  return ret
end

function _QB.animation.fromsheet (sheet, index_start, index_end, frame_speed, frame_repeat, keyframe)
  local frame_speed = frame_speed or 1.0
  local frame_repeat = frame_repeat or false
  local keyframe = keyframe or 0

  local ret = { _animation=_QB.animation_fromsheet(sheet._sprite, index_start, index_end, frame_speed, frame_repeat, keyframe) }
  setmetatable(ret, _QB.animation.Animation)
  return ret
end

function _QB.animation.Animation:play ()
  local ret = { _sprite=_QB.animation_play(self._animation) }
  setmetatable(ret, _QB.sprite.Sprite)
  return ret
end

function _QB.animation.Animation:getoffset ()
  return _QB.animation.animation_getoffset(self._animation)
end

function _QB.animation.Animation:setoffset (ox, oy)
  _QB.animation.animation_setoffset(self._animation, ox, oy)
end

_QB.window = {}
_QB.window.fullscreen_type = {
  WINDOWED='windowed',
  WINDOW_FULLSCREEN='window_fullscreen',
  WINDOW_FULLSCREEN_DESKTOP='window_fullscreen_desktop',
}

function _QB.window.width()
  return _QB.window_width()
end

function _QB.window.height()
  return _QB.window_height()
end

function _QB.window.getfullscreen()
  _QB.window_getfullscreen()
end

function _QB.window.setfullscreen(type)
  _QB.window_setfullscreen(type)
end

function _QB.window.getbordered()
  _QB.window_getbordered()
end

function _QB.window.setbordered(bordered)
  _QB.window_setbordered(bordered)
end

)";

void lua_start(lua_State* L) {
  if (!L) {
    return;
  }

  // Call qb.init(width, height)
  lua_getglobal(L, "qb");
  lua_getfield(L, -1, "start");
  if (!lua_isnil(L, -1)) {
    lua_pushnumber(L, qb_window_width());
    lua_pushnumber(L, qb_window_height());
    lua_call(L, 2, 0);
    lua_pop(L, 1);
  } else {
    lua_pop(L, 2);
  }
}

void lua_update(lua_State* L) {
  if (!L) {
    return;
  }

  // Call qb.udpate()
  lua_getglobal(L, "qb");
  lua_getfield(L, -1, "update");
  if (!lua_isnil(L, -1)) {
    lua_call(L, 0, 0);
    lua_pop(L, 1);
  } else {
    lua_pop(L, 2);
  }
}

void lua_draw(lua_State* L) {
  if (!L) {
    return;
  }
  return;
  // Call qb.draw()
  lua_getglobal(L, "qb");
  lua_getfield(L, -1, "draw");
  if (!lua_isnil(L, -1)) {
    lua_call(L, 0, 0);
    lua_pop(L, 1);
  } else {
    lua_pop(L, 2);
  }  
}

void lua_resize(lua_State* L, uint32_t width, uint32_t height) {
  if (!L) {
    return;
  }

  // Call qb.resize()
  lua_getglobal(L, "qb");
  lua_getfield(L, -1, "resize");
  if (!lua_isnil(L, -1)) {
    lua_pushinteger(L, width);
    lua_pushinteger(L, height);
    lua_call(L, 2, 0);
    lua_pop(L, 1);
  } else {
    lua_pop(L, 2);
  }
}

void lua_init(lua_State* L) {
  if (!L) {
    return;
  }

  // Call qb.init()
  lua_getglobal(L, "qb");
  lua_getfield(L, -1, "init");
  if (!lua_isnil(L, -1)) {
    lua_call(L, 0, 0);
    lua_pop(L, 1);
  } else {
    lua_pop(L, 2);
  }
}

std::filesystem::path directory;
std::filesystem::path entrypoint;

void lua_bindings_initialize(struct qbScriptAttr_* attr) {
  directory = std::filesystem::path(qb_resources()->dir) / qb_resources()->scripts;
  entrypoint = attr->entrypoint ? directory / attr->entrypoint : directory / "main.lua";

  lua_input_initialize();
}

lua_State* lua_thread_initialize() {
  std::cout << "Initializing Lua\n";
  lua_State* L = luaL_newstate();

  luaL_openlibs(L);
  lua_newtable(L);
  luaL_setfuncs(L, qb_lib, 0);
  lua_setglobal(L, QB_LUA_LIB);

  if (luaL_dostring(L, QB_LUA_INIT) != LUA_OK) {
    std::cout << lua_tostring(L, -1) << std::endl;
  }

  if (luaL_dofile(L, entrypoint.generic_string().c_str()) != LUA_OK) {
    std::cout << lua_tostring(L, -1) << std::endl;
    lua_close(L);
    return nullptr;
  }

  lua_init(L);

  return L;
}