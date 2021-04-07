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
#include <cubez/render_pipeline.h>
#include <cglm/struct.h>

typedef struct qbSprite_* qbSprite;
typedef struct qbAnimationBuilder_* qbAnimationBuilder;
typedef struct qbAnimation_* qbAnimation;
typedef struct qbAnimator_* qbAnimator;

QB_API qbSprite qb_sprite_load(const char* filename);

QB_API qbSprite qb_spritesheet_load(const char* filename, int tw, int th, int margin);

QB_API qbSprite qb_sprite_fromsheet(qbSprite sheet, int ix, int iy);

QB_API qbSprite qb_sprite_frompixels(qbPixelMap pixels);

QB_API qbSprite qb_sprite_fromimage(qbImage image);

QB_API void     qb_sprite_draw(qbSprite sprite, vec2s pos);

QB_API void     qb_sprite_draw_ext(qbSprite sprite, vec2s pos,
                                   vec2s scale, float rot, vec4s col);

QB_API void     qb_sprite_drawpart(qbSprite sprite, vec2s pos, int32_t left, int32_t top, int32_t width, int32_t height);

QB_API void     qb_sprite_drawpart_ext(qbSprite sprite, vec2s pos, int32_t left, int32_t top, int32_t width, int32_t height,
                                       vec2s scale, float rot, vec4s col);

QB_API vec2s    qb_sprite_getoffset(qbSprite sprite);
QB_API void     qb_sprite_setoffset(qbSprite sprite, vec2s offset);

QB_API uint32_t qb_sprite_width(qbSprite sprite);
QB_API uint32_t qb_sprite_height(qbSprite sprite);

QB_API void     qb_sprite_setdepth(qbSprite sprite, float depth);
QB_API float    qb_sprite_getdepth(qbSprite sprite);

QB_API int32_t qb_sprite_framecount(qbSprite sprite);
QB_API int32_t qb_sprite_getframe(qbSprite sprite);
QB_API void     qb_sprite_setframe(qbSprite sprite, int32_t frame_index);
QB_API qbImage  qb_sprite_subimg(qbSprite sprite, int32_t frame);

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

  vec2s offset;
} qbAnimationAttr_, *qbAnimationAttr;

QB_API qbAnimation qb_animation_create(qbAnimationAttr attr);

QB_API qbAnimation qb_animation_loaddir(const char* dir, qbAnimationAttr attr);

QB_API qbAnimation qb_animation_fromsheet(qbAnimationAttr attr, qbSprite sheet,
                                          int index_start, int index_end);

QB_API qbSprite  qb_animation_play(qbAnimation animation);

QB_API vec2s  qb_animation_getoffset(qbAnimation animation);
QB_API void   qb_animation_setoffset(qbAnimation animation, vec2s offset);

// Component type: qbSprite
QB_API qbComponent qb_sprite();

#endif  // CUBEZ_SPRITE__H