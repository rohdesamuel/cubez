/**
* Author: Samuel Rohde (rohde.samuel@cubez.io)
*
* Copyright 2021 Samuel Rohde
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

#ifndef CUBEZ_SPRITE__H
#define CUBEZ_SPRITE__H

#include <cubez/cubez.h>
#include <cglm/struct.h>

typedef struct qbSprite_* qbSprite;
typedef struct qbSpriteAtlas_* qbSpriteAtlas;

QB_API qbSpriteAtlas qb_spriteatlas_load(const char* atlas_name, const char* filename, int tw, int th);

QB_API qbSprite qb_sprite_load(const char* sprite_name, const char* filename);

QB_API qbSprite qb_sprite_fromatlas(const char* sprite_name, qbSpriteAtlas atlas, int ix, int iy);

QB_API qbSprite qb_sprite_find(const char* sprite_name);

QB_API void     qb_sprite_draw(qbSprite sprite, int32_t subimg, vec2s pos);

QB_API void     qb_sprite_draw_ext(qbSprite sprite, int32_t subimg, vec2s pos,
                                   vec2s scale, float rot, vec4s col);

QB_API void     qb_sprite_setdepth(qbSprite sprite, float depth);
QB_API float    qb_sprite_getdepth(qbSprite sprite);

QB_API void     qb_sprite_initialize(uint32_t width, uint32_t height);
QB_API void     qb_sprite_onresize(uint32_t width, uint32_t height);
QB_API void     qb_sprite_flush(qbFrameBuffer frame);

#endif  // CUBEZ_SPRITE__H