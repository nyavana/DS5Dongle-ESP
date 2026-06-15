//
// Created by awalol on 2026/3/5.  (ESP-IDF port scaffold)
//

#ifndef DS5_BRIDGE_AUDIO_H
#define DS5_BRIDGE_AUDIO_H

#include <cstddef>
#include <cstdint>

void audio_init();
void audio_loop();
void core1_entry();
void set_headset(bool state);

// Build a DualSense 0x36 report from already-prepared haptics and speaker
// payloads. UAC capture/resampling/Opus producers are deferred until S31
// hardware validates HID+UAC enumeration and timing.
bool audio_build_report(uint8_t *report,
                        std::size_t report_len,
                        const uint8_t *haptics,
                        std::size_t haptics_len,
                        const uint8_t *speaker,
                        std::size_t speaker_len);

#endif //DS5_BRIDGE_AUDIO_H
