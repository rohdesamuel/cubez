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
qbHandle jump_playing = qb_audio_play(jump_wav);
qb_audio_setvolume(jump_playing, 0.4f, 0.4f);
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


typedef struct qbAudioLoadAttr_ {
  // Sets the volums for the sound, 0.f for quiet, 1.f for full volume.
  // Default 1.f
  float volume;

  // Sets the pan, 0.f for full left, 1.f for full right.
  // Default 0.5f
  float pan;

  // Enables looping.
  // Default QB_FALSE
  qbBool loop;
} qbAudioLoadAttr_, *qbAudioLoadAttr;
// Loads the WAV file in the resources directory. Returns NULL if file is not found.
QB_API qbAudioBuffer qb_audio_loadwav(const utf8_t* file, qbAudioLoadAttr opt_attr);
QB_API void qb_audio_free(qbAudioBuffer loaded);

// Starts playing the loaded sound. The pan, volume, and loop defaults are set
// from the loaded sample. This can be overridden when the sample is playing
// without changing other playing samples.
QB_API qbHandle qb_audio_play(qbAudioBuffer loaded);

// Stops playing the sample.
QB_API void qb_audio_stop(qbHandle playing);

// Pauses the playing sample.
QB_API void qb_audio_pause(qbHandle playing);

// Unpases the playing sample.
QB_API void qb_audio_unpause(qbHandle playing);

// Sets looping the sample.
QB_API void qb_audio_loop(qbHandle playing, int enable_looping);

// Sets the pan of the sample, 0.f for left, 1.f for right.
QB_API void qb_audio_setpan(qbHandle playing, float pan);

// Sets the volume of the sample.
QB_API void qb_audio_setvolume(qbHandle playing, float volume);

// Returns true if the sample is playing, and false if it stopped.
QB_API qbBool qb_audio_isplaying(qbHandle playing);

// Stops playing all samples.
QB_API void qb_audio_stopall();

#endif  // CUBEZ_AUDIO__H