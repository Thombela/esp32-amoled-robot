#pragma once

// Optional audio output — used to chime when the Claude session limit resets.
// The embedded chime engine (ES8311 codec + PCM playback) was removed to save
// flash, so every board's sound.cpp is currently a no-op on init/tick and
// ignores play requests.
//
// Playback is non-blocking: sound_hal_play_reset() only *queues* the chime and
// returns immediately; sound_hal_tick() (called every loop) advances the notes
// so the LVGL render loop never stalls.

void sound_hal_init(void);
void sound_hal_tick(void);
void sound_hal_play_reset(void);
