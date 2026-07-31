#include "../../hal/sound_hal.h"

// AMOLED-2.16 has an ES8311 codec + speaker, but the chime engine (embedded
// PCM + codec driver) was removed to save flash — sound output is a no-op.

void sound_hal_init(void) {}
void sound_hal_tick(void) {}
void sound_hal_play_reset(void) {}
