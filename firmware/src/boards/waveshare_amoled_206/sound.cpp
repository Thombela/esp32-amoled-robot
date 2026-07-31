#include "../../hal/sound_hal.h"

// AMOLED-2.06: an ES8311 codec (plus an ES7210 ADC) sits on the shared I2C bus,
// but this port never brings that path up and it's unverified on hardware —
// so sound output is a no-op. The firmware's chime engine (shared ES8311 +
// embedded PCM playback) has since been removed entirely to save flash.

void sound_hal_init(void) {}
void sound_hal_tick(void) {}
void sound_hal_play_reset(void) {}
