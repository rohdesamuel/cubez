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

#ifndef AUDIO_INTERNAL__H
#define AUDIO_INTERNAL__H

#include <cubez/common.h>

struct AudioSettings {
  uint32_t sample_frequency;
  uint32_t buffered_samples;
};

void audio_initialize(const AudioSettings& settings);
void audio_shutdown();

#endif  // AUDIO_INTERNAL__H