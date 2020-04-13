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
  uint32_t sample_frequency;
  uint32_t buffered_samples;
} qbAudioAttr_, *qbAudioAttr;

typedef struct qbAudioBuffer_ *qbAudioBuffer;
typedef struct qbAudioPlaying_ *qbAudioPlaying;

typedef enum qbAudoLoop {
  QB_AUDIO_LOOP_DISABLE = 0,
  QB_AUDIO_LOOP_ENABLE = 1
} qbAudoLoop;

QB_API qbAudioBuffer qb_audio_loadwav(const char* file);
QB_API qbAudioPlaying qb_audio_upload(qbAudioBuffer loaded);

QB_API void qb_audio_free(qbAudioBuffer loaded);
QB_API int qb_audio_size(qbAudioBuffer loaded);

QB_API int qb_audio_isplaying(qbAudioPlaying playing);
QB_API void qb_audio_play(qbAudioPlaying playing);
QB_API void qb_audio_stop(qbAudioPlaying playing);
QB_API void qb_audio_loop(qbAudioPlaying playing, qbAudoLoop enable_loop);
QB_API void qb_audio_pause(qbAudioPlaying playing);
QB_API void qb_audio_pan(qbAudioPlaying playing, float pan);
QB_API void qb_audio_volume(qbAudioPlaying playing, float left, float right);

QB_API void qb_audio_stopall();

#endif  // CUBEZ_AUDIO__H