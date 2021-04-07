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

#include <cubez/audio.h>

#include <filesystem>

#include "audio_internal.h"
#include "cute_sound.h"

#include "sparse_map.h"
#include "block_vector.h"

namespace fs = std::experimental::filesystem;

// https://www.reddit.com/r/gamedev/comments/6i39j2/tinysound_the_cutest_library_to_get_audio_into/
cs_context_t* ctx;

typedef struct qbAudioBuffer_ {
  qbId id;
  cs_loaded_sound_t loaded;
} qbAudioLoaded_, *qbAudioBuffer;

typedef struct qbAudioPlaying_ {
  qbId id;
  cs_playing_sound_t playing;
  int paused;
  qbAudioBuffer buf;
} qbAudioPlaying_, *qbAudioPlaying;

qbId sound_id;
SparseMap<qbAudioPlaying_, TypedBlockVector<qbAudioPlaying_>> sounds_;
SparseMap<qbAudioLoaded_, TypedBlockVector<qbAudioLoaded_>> loaded_;
std::vector<qbId> unused_ids;

void audio_initialize(qbAudioAttr settings) {
  qbAudioAttr_ attr{};
  attr.buffered_samples = 8192;
  attr.sample_frequency = 44100;
  if (!settings) {
    settings = &attr;
  }

  int use_playing_pool = 0; // non-zero uses high-level API, 0 uses low-level API
  int num_elements_in_playing_pool = use_playing_pool ? 5 : 0; // pooled memory array size for playing sounds
  void* no_allocator_used = NULL; // No custom allocator is used, just pass in NULL here.
  ctx = cs_make_context(0, settings->sample_frequency, settings->buffered_samples, num_elements_in_playing_pool, no_allocator_used);
  sound_id = 0;
  cs_spawn_mix_thread(ctx);
}

void audio_shutdown() {
  if (!ctx) {
    return;
  }
  cs_shutdown_context(ctx);
}

qbAudioBuffer qb_audio_loadwav(const char* file) {
  fs::path path = fs::path(qb_resources()->dir) / fs::path(qb_resources()->sounds) / file;
  if (fs::exists(path)) {
    qbId id = sound_id;
    loaded_.insert(id, qbAudioLoaded_{ sound_id, cs_load_wav(path.string().c_str()) });
    return &loaded_[id];
  } else {
    std::cerr << "Could not load sound: \"" << path << "\"";
  }
  return nullptr;
}

qbAudioPlaying qb_audio_upload(qbAudioBuffer loaded) {
  qbId id = sound_id;
  qbAudioPlaying_ sound = { sound_id, cs_make_playing_sound(&loaded->loaded), 1, loaded };
  sounds_.insert(id, sound);
  return &sounds_[id];
}

qbAudioBuffer qb_audio_buffer(qbAudioPlaying playing) {
  return playing->buf;
}

void qb_audio_free(qbAudioBuffer loaded) {
  if (sounds_.has(loaded->id)) {
    cs_stop_sound(&sounds_[loaded->id].playing);
    sounds_.erase(loaded->id);
  }
  cs_free_sound(&loaded_[loaded->id].loaded);
  loaded_.erase(loaded->id);
}

int qb_audio_getsize(qbAudioBuffer loaded) {
  return cs_sound_size(&loaded->loaded);
}

void qb_audio_play(qbAudioPlaying playing) {
  if (!cs_is_active(&playing->playing)) {
    cs_insert_sound(ctx, &playing->playing);
  }
}

void qb_audio_stop(qbAudioPlaying playing) {
  cs_stop_sound(&playing->playing);
}

int qb_audio_isplaying(qbAudioPlaying playing) {
  return cs_is_active(&playing->playing);
}

void qb_audio_loop(qbAudioPlaying playing, qbAudoLoop enable_loop) {
  if (cs_is_active(&playing->playing)) {
    cs_loop_sound(&playing->playing, (int)enable_loop);
  }
}

void qb_audio_pause(qbAudioPlaying playing) {
  if (cs_is_active(&playing->playing)) {
    cs_pause_sound(&playing->playing, playing->paused);
  }
}

void qb_audio_setpan(qbAudioPlaying playing, float pan) {
  if (cs_is_active(&playing->playing)) {
    cs_set_pan(&playing->playing, pan);
  }
}

void qb_audio_setvolume(qbAudioPlaying playing, float left, float right) {
  if (cs_is_active(&playing->playing)) {
    cs_set_volume(&playing->playing, left, right);
  }
}

void qb_audio_stopall() {
  cs_stop_all_sounds(ctx);
}