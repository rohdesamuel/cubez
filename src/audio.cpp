#include <cubez/audio.h>

#include "audio_internal.h"
#include "cute_sound.h"

#include "sparse_map.h"
#include "block_vector.h"

cs_context_t* ctx;

typedef struct qbAudioLoaded_ {
  qbId id;
  cs_loaded_sound_t loaded;
} qbAudioLoaded_, *qbAudioLoaded;

typedef struct qbAudioPlaying_ {
  qbId id;
  cs_playing_sound_t playing;
  int paused;
} qbAudioPlaying_, *qbAudioPlaying;

qbId sound_id;
SparseMap<qbAudioPlaying_, TypedBlockVector<qbAudioPlaying_>> sounds_;
SparseMap<qbAudioLoaded_, TypedBlockVector<qbAudioLoaded_>> loaded_;
std::vector<qbId> unused_ids;

void audio_initialize(const AudioSettings& settings) {
  int buffered_samples = 8192; // number of samples internal buffers can hold at once
  int use_playing_pool = 0; // non-zero uses high-level API, 0 uses low-level API
  int num_elements_in_playing_pool = use_playing_pool ? 5 : 0; // pooled memory array size for playing sounds
  void* no_allocator_used = NULL; // No custom allocator is used, just pass in NULL here.
  ctx = cs_make_context(0, settings.sample_frequency, buffered_samples, num_elements_in_playing_pool, no_allocator_used);
  sound_id = 0;
  cs_spawn_mix_thread(ctx);
/*
  cs_loaded_sound_t loaded = cs_load_wav("resources/jump.wav");

  cs_playing_sound_t jump = cs_make_playing_sound(&loaded);
  cs_spawn_mix_thread(ctx);

  cs_sleep(500);

  int count = 10;
  while (count--) {
    cs_sleep(500);
    cs_insert_sound(ctx, &jump);
  }

  cs_sleep(500);
  cs_free_sound(&loaded);
  */
}

void audio_shutdown() {
  cs_shutdown_context(ctx);
}

qbAudioLoaded qb_audio_loadwav(const char* file) {
  qbId id = sound_id;
  loaded_.insert(id, qbAudioLoaded_{ sound_id, cs_load_wav(file) });
  return &loaded_[id];
}

qbAudioPlaying qb_audio_ready(qbAudioLoaded loaded) {
  qbId id = sound_id;
  qbAudioPlaying_ sound = { sound_id, cs_make_playing_sound(&loaded->loaded) };
  sounds_.insert(id, sound);
  return &sounds_[id];
}

void qb_audio_free(qbAudioLoaded loaded) {
  if (sounds_.has(loaded->id)) {
    cs_stop_sound(&sounds_[loaded->id].playing);
    sounds_.erase(loaded->id);
  }
  cs_free_sound(&loaded_[loaded->id].loaded);
  loaded_.erase(loaded->id);
}

int qb_audio_size(qbAudioLoaded loaded) {
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

void qb_audio_pan(qbAudioPlaying playing, float pan) {
  if (cs_is_active(&playing->playing)) {
    cs_set_pan(&playing->playing, pan);
  }
}

void qb_audio_volume(qbAudioPlaying playing, float left, float right) {
  if (cs_is_active(&playing->playing)) {
    cs_set_volume(&playing->playing, left, right);
  }
}

void qb_audio_stopall() {
  cs_stop_all_sounds(ctx);
}