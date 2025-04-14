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

#ifndef CUBEZ_GUI__H
#define CUBEZ_GUI__H

#include <cubez/nuklear.h>

QB_API struct nk_context* nk_ctx();

QB_API void nk_font_stash_begin(struct nk_font_atlas** atlas);

QB_API void nk_font_stash_end(void);

QB_API struct nk_font* nk_font_atlas_add_from_file(struct nk_font_atlas* atlas, const char* file_path, float height, const struct nk_font_config*);

QB_API char* nk_file_load(const char* path, nk_size* siz, struct nk_allocator* alloc);

#endif  // CUBEZ_GUI__H