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

#ifndef CUBEZ_WINDOW__H
#define CUBEZ_WINDOW__H

#include <cubez/common.h>
#include <cglm/cglm.h>
#include <cglm/types-struct.h>

enum qbFullscreenType {
  QB_FULLSCREEN_TYPE_WINDOWED,
  QB_FULLSCREEN_TYPE_FULLSCREEN,
  QB_FULLSCREEN_TYPE_BORDERLESS
};

QB_API uint32_t qb_window_width();
QB_API uint32_t qb_window_height();

QB_API void qb_window_resize(uint32_t width, uint32_t height);
QB_API void qb_window_setfullscreen(qbFullscreenType type);
QB_API qbFullscreenType  qb_window_fullscreen();
QB_API void qb_window_setbordered(qbBool is_bordered);
QB_API void qb_window_setresizeable(qbBool is_resizeable);
QB_API qbBool qb_window_bordered();
QB_API qbBool qb_window_resizeable();
QB_API void qb_window_settransparency(float alpha);
QB_API void qb_window_settransparencycolor(vec3s rgb);
QB_API void qb_window_setalwaysontop(qbBool is_on_top);

#endif  // CUBEZ_WINDOW__H