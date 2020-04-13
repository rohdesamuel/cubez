/**
* Author: Samuel Rohde (rohde.samuel@cubez.io)
*
* Copyright {2020} {Samuel Rohde}
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

#ifndef GUI_INTERNAL__H
#define GUI_INTERNAL__H

#include <cubez/input.h>

void gui_initialize();
void gui_shutdown();

void gui_handle_input(qbInputEvent input);
qbRenderPass gui_create_renderpass(uint32_t width, uint32_t height);
void gui_window_updateuniforms();

#endif  // GUI_INTERNAL__H