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

#ifndef CUBEZ_CAMERA__H
#define CUBEZ_CAMERA__H

#include <cubez/cubez.h>
#include <cubez/render_pipeline.h>
#include <cglm/cglm.h>
#include <cglm/types-struct.h>

typedef struct qbCamera_ {
  float aspect;
  float near;
  float far;
  float fov;

  vec3s eye;
  mat4s view_mat;
  mat4s projection_mat;
} qbCamera_, * qbCamera;

QB_API qbCamera qb_camera_ortho(float left, float right, float bottom, float top, vec2s eye);
QB_API qbCamera qb_camera_perspective(float fov, float aspect, float near, float far, vec3s eye, vec3s center, vec3s up);
QB_API void qb_camera_resize(qbCamera camera, uint32_t width, uint32_t height);
QB_API void qb_camera_destroy(qbCamera* camera);
QB_API vec3s qb_camera_screentoworld(qbCamera camera, vec2s screen);
QB_API vec2s qb_camera_worldtoscreen(qbCamera camera, vec3s world);

#endif  // CUBEZ_CAMERA__H