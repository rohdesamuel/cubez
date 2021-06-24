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

#ifndef CUBEZ_AUDIO__H
#define CUBEZ_AUDIO__H

#include <cubez/common.h>

/*

Example:

qbAudioBuffer jump_wav = qb_audio_loadwav("resources/jump.wav");
qbAudioPlaying jump = qb_audio_upload(jump_wav);
qb_audio_play(jump);
...

// Stops the wav and frees the audio.
qb_audio_free(jump);

*/

typedef struct qbAudioAttr_ {
  // Audio sampling frequency.
  // Default 44100Hz
  uint32_t sample_frequency;

  // Number of samples internal buffers can hold at once.
  // Default 8192 samples.
  uint32_t buffered_samples;
} qbAudioAttr_, *qbAudioAttr;

typedef struct qbAudioBuffer_ *qbAudioBuffer;

// Loads the WAV file in the resources directory.
QB_API qbAudioBuffer qb_audio_loadwav(const char* file);

QB_API void qb_audio_free(qbAudioBuffer loaded);
QB_API size_t qb_audio_getsize(qbAudioBuffer loaded);

QB_API qbHandle qb_audio_play(qbAudioBuffer loaded);
QB_API void qb_audio_stop(qbHandle playing);
QB_API void qb_audio_loop(qbHandle playing, int enable_looping);
QB_API void qb_audio_pause(qbHandle playing);
QB_API void qb_audio_setpan(qbHandle playing, float pan);
QB_API void qb_audio_setvolume(qbHandle playing, float left, float right);
QB_API qbBool qb_audio_isplaying(qbHandle playing);

QB_API void qb_audio_stopall();

#endif  // CUBEZ_AUDIO__H