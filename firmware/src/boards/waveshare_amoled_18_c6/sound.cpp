#include "../../hal/sound_hal.h"

// C6 AMOLED-1.8: an ES8311 codec sits on the shared I2C bus and the audio amp
// is gated by TCA9554 P7, but this port never brings that path up (P7 is left
// off in io_expander_init) and it's unverified on hardware — so sound output
// is a no-op. The firmware's chime engine (shared ES8311 + embedded PCM
// playback) has since been removed entirely to save flash.

void sound_hal_init(void) {}
void sound_hal_tick(void) {}
void sound_hal_play_reset(void) {}
