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
typedef struct qbAnimationBuilder_* qbAnimationBuilder;
typedef struct qbAnimation_* qbAnimation;
typedef struct qbAnimator_* qbAnimator;

QB_API qbSprite qb_sprite_load(const char* sprite_name, const char* filename);

QB_API qbSprite qb_spritesheet_load(const char* sheet_name, const char* filename, int tw, int th, int margin);

QB_API qbSprite qb_sprite_fromsheet(const char* sprite_name, qbSprite sheet, int ix, int iy);

QB_API qbSprite qb_sprite_find(const char* sprite_name);

QB_API void     qb_sprite_draw(qbSprite sprite, vec2s pos);

QB_API void     qb_sprite_draw_ext(qbSprite sprite, vec2s pos,
                                   vec2s scale, float rot, vec4s col);

QB_API void     qb_sprite_setoffset(qbSprite sprite, vec2s offset);

QB_API uint32_t qb_sprite_width(qbSprite sprite);
QB_API uint32_t qb_sprite_height(qbSprite sprite);

QB_API void     qb_sprite_setdepth(qbSprite sprite, float depth);
QB_API float    qb_sprite_getdepth(qbSprite sprite);

QB_API void     qb_sprite_initialize(uint32_t width, uint32_t height);
QB_API void     qb_sprite_onresize(uint32_t width, uint32_t height);
QB_API void     qb_sprite_flush(qbFrameBuffer frame, qbRenderEvent e);

typedef struct qbAnimationAttr_ {
  qbSprite* frames;

  // Unit in milliseconds.
  double* durations;
  size_t frame_count;

  // Unit in milliseconds.
  double frame_speed;

  bool repeat;
  int keyframe;
} qbAnimationAttr_, *qbAnimationAttr;

QB_API qbAnimation qb_animation_create(const char* animation_name, qbAnimationAttr attr);

QB_API qbAnimation qb_animation_fromsheet(const char* animation_name,
                                          qbAnimationAttr attr, qbSprite sheet,
                                          int index_start, int index_end);

QB_API qbSprite  qb_animation_play(qbAnimation animation);

// Component type: qbSprite
QB_API qbComponent qb_sprite();

#endif  // CUBEZ_SPRITE__H