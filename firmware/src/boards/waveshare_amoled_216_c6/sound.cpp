#include "../../hal/sound_hal.h"

// C6 AMOLED-2.16 has no buzzer wired, so sound output is a no-op.

void sound_hal_init(void) {}
void sound_hal_tick(void) {}
void sound_hal_play_reset(void) {}
