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

#include <atomic>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include "audio_internal.h"
#include "cute_sound.h"

#include "sparse_map.h"
#include "block_vector.h"

namespace fs = std::filesystem;

// https://www.reddit.com/r/gamedev/comments/6i39j2/tinysound_the_cutest_library_to_get_audio_into/
cs_context_t* ctx;

typedef struct qbAudioBuffer_ {
  qbId id;
  cs_loaded_sound_t loaded;
} qbAudioBuffer_, *qbAudioBuffer;

typedef struct qbAudioPlaying_ {
  qbId id;
  cs_playing_sound_t playing;
  int paused;
  qbAudioBuffer buf;
} qbAudioPlaying_, *qbAudioPlaying;

std::mutex sounds_mu_;
std::mutex loaded_mu_;

std::atomic<qbId> sound_id;
std::unordered_map<qbId, qbAudioPlaying> sounds_ /* GUARDED_BY(sounds_mu_) */;
std::unordered_set<qbAudioBuffer> loaded_ /* GUARDED_BY(loaded_mu_) */;

constexpr std::chrono::duration<int64_t> CLEANUP_THREAD_SLEEP = std::chrono::seconds(30);

void cleanup_thread() {
  while (qb_running()) {
    std::unordered_map<qbId, qbAudioPlaying> sounds;
    {
      std::lock_guard<decltype(sounds_mu_)> l(sounds_mu_);
      sounds = sounds_;
    }

    std::vector<qbId> to_delete;
    for (auto& p : sounds) {
      if (!cs_is_active(&p.second->playing)) {
        to_delete.push_back(p.first);
      }
    }

    {
      std::lock_guard<decltype(sounds_mu_)> l(sounds_mu_);
      for (auto id : to_delete) {
        sounds_.erase(id);
      }
    }
    std::this_thread::sleep_for(CLEANUP_THREAD_SLEEP);
  }
}

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

  std::thread t(cleanup_thread);
  t.detach();

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
    std::lock_guard<decltype(loaded_mu_)> l(loaded_mu_);
    qbAudioBuffer ret = new qbAudioBuffer_{ sound_id, cs_load_wav(path.string().c_str()) };
    loaded_.insert(ret);
    return ret;
  } else {
    std::cerr << "Could not load sound: \"" << path << "\"";
  }
  return nullptr;
}

void qb_audio_free(qbAudioBuffer loaded) {
  cs_free_sound(&loaded->loaded);

  {
    std::lock_guard<decltype(loaded_mu_)> l(loaded_mu_);
    loaded_.erase(loaded);
  }
}

size_t qb_audio_getsize(qbAudioBuffer loaded) {
  return (size_t)cs_sound_size(&loaded->loaded);
}

qbHandle qb_audio_play(qbAudioBuffer loaded) {
  qbId id;
  qbAudioPlaying sound;
  {
    std::lock_guard<decltype(sounds_mu_)> l(sounds_mu_);
    id = sound_id++;
    sound = new qbAudioPlaying_{ sound_id, cs_make_playing_sound(&loaded->loaded), 1, loaded };
    sounds_[id] = sound;
  }
  cs_insert_sound(ctx, &sound->playing);

  return{ id };
}

void qb_audio_stop(qbHandle playing) {
  std::lock_guard<decltype(sounds_mu_)> l(sounds_mu_);

  auto found = sounds_.find(playing);
  if (found != sounds_.end()) {
    cs_playing_sound_t* playing = &found->second->playing;
    if (!cs_is_active(playing)) {
      cs_stop_sound(&found->second->playing);
    }
  }
}

qbBool qb_audio_isplaying(qbHandle playing) {
  std::lock_guard<decltype(sounds_mu_)> l(sounds_mu_);

  auto found = sounds_.find(playing);
  if (found != sounds_.end()) {
    return cs_is_active(&found->second->playing);
  }
  return QB_FALSE;
}

void qb_audio_loop(qbHandle playing, int enable_looping) {
  std::lock_guard<decltype(sounds_mu_)> l(sounds_mu_);

  auto found = sounds_.find(playing);
  if (found != sounds_.end()) {
    cs_playing_sound_t* playing = &found->second->playing;
    if (cs_is_active(playing)) {
      cs_loop_sound(playing, enable_looping);
    }
  }
}

void qb_audio_pause(qbHandle playing) {
  std::lock_guard<decltype(sounds_mu_)> l(sounds_mu_);

  auto found = sounds_.find(playing);
  if (found != sounds_.end()) {
    cs_playing_sound_t* playing = &found->second->playing;
    if (cs_is_active(playing)) {
      cs_pause_sound(playing, playing->paused);
    }
  }
}

void qb_audio_setpan(qbHandle playing, float pan) {
  std::lock_guard<decltype(sounds_mu_)> l(sounds_mu_);

  auto found = sounds_.find(playing);
  if (found != sounds_.end()) {
    cs_playing_sound_t* playing = &found->second->playing;
    if (cs_is_active(playing)) {
      cs_set_pan(playing, pan);
    }
  }
}

void qb_audio_setvolume(qbHandle playing, float left, float right) {
  std::lock_guard<decltype(sounds_mu_)> l(sounds_mu_);

  auto found = sounds_.find(playing);
  if (found != sounds_.end()) {
    cs_playing_sound_t* playing = &found->second->playing;
    if (cs_is_active(playing)) {
      cs_set_volume(playing, left, right);
    }
  }
}

void qb_audio_stopall() {
  cs_stop_all_sounds(ctx);
}