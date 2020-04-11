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