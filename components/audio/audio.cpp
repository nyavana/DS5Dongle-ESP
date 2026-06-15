//
// Audio bridge scaffold (ported from legacy-pico/src/audio.cpp).
//
// The Pico build consumed TinyUSB UAC frames, resampled channels 3/4 to a
// 3 kHz haptics block, Opus-encoded speaker/headset audio on core 1, and sent
// DualSense 0x36 reports over Bluetooth. On the autonomous ESP32-S31 build the
// runtime UAC/Opus path stays disabled until real hardware validates HID+UAC
// composite enumeration and timing. This component preserves the API, task /
// queue shape, headset state, guarded speaker buffer, counters, and 0x36 packet
// layout so the future runtime path has a stable landing point.
//

#include "audio.h"

#include <algorithm>
#include <cstring>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "bridge.h"
#include "bt.h"
#include "config.h"
#include "state_mgr.h"

namespace {

constexpr std::size_t kHapticsPayloadLen = 64;
constexpr std::size_t kSpeakerPayloadLen = 200;
constexpr std::size_t kReportLen = 398;
constexpr std::size_t kOutputStateLen = 63;
constexpr UBaseType_t kAudioQueueDepth = 2;
constexpr uint32_t kAudioTaskStack = 4096;

constexpr uint8_t kReportId = 0x36;
constexpr uint8_t kAudioHeaderTag = 0x11 | (1 << 7);
constexpr uint8_t kStateTag = 0x10 | (1 << 7);
constexpr uint8_t kHapticsTag = 0x12 | (1 << 7);
constexpr uint8_t kSpeakerTag = 0x13 | (1 << 7);
constexpr uint8_t kHeadsetTag = 0x16 | (1 << 7);

struct AudioQueueItem {
    uint8_t haptics[kHapticsPayloadLen];
    uint8_t speaker[kSpeakerPayloadLen];
    bool has_speaker;
};

const char *TAG = "audio";

QueueHandle_t s_audio_queue = nullptr;
TaskHandle_t s_audio_task = nullptr;
portMUX_TYPE s_audio_mux = portMUX_INITIALIZER_UNLOCKED;

uint8_t s_report_seq = 0;
uint8_t s_packet_counter = 0;
bool s_headset = false;
bool s_runtime_enabled = false;
uint8_t s_latest_speaker[kSpeakerPayloadLen]{};
bool s_latest_speaker_valid = false;

void snapshot_audio_state(uint8_t *seq,
                          uint8_t *packet,
                          bool *headset,
                          uint8_t *speaker,
                          bool *speaker_valid) {
    taskENTER_CRITICAL(&s_audio_mux);
    *seq = s_report_seq & 0x0f;
    s_report_seq = (s_report_seq + 1) & 0x0f;
    *packet = s_packet_counter++;
    *headset = s_headset;
    *speaker_valid = s_latest_speaker_valid;
    std::memcpy(speaker, s_latest_speaker, kSpeakerPayloadLen);
    taskEXIT_CRITICAL(&s_audio_mux);
}

void audio_task(void *arg) {
    (void) arg;
    uint8_t report[kReportLen]{};
    AudioQueueItem item{};

    while (true) {
        if (xQueueReceive(s_audio_queue, &item, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        if (!audio_build_report(report,
                                sizeof(report),
                                item.haptics,
                                sizeof(item.haptics),
                                item.has_speaker ? item.speaker : nullptr,
                                item.has_speaker ? sizeof(item.speaker) : 0)) {
            ESP_LOGW(TAG, "dropped prepared audio frame");
            continue;
        }

        if (s_runtime_enabled) {
            bt_write(report, sizeof(report));
        }
    }
}

} // namespace

void set_headset(bool state) {
    taskENTER_CRITICAL(&s_audio_mux);
    s_headset = state;
    taskEXIT_CRITICAL(&s_audio_mux);
}

void audio_loop() {
    // UAC capture is intentionally deferred. The Pico loop read host audio here;
    // the S31 scaffold keeps that path inert so HID-only output reports continue.
}

void core1_entry() {
    // The Pico core-1 worker performed Opus encode. ESP-IDF will use a FreeRTOS
    // task and esp_audio_codec after hardware validates the UAC runtime path.
}

bool audio_build_report(uint8_t *report,
                        const std::size_t report_len,
                        const uint8_t *haptics,
                        const std::size_t haptics_len,
                        const uint8_t *speaker,
                        const std::size_t speaker_len) {
    if (report == nullptr || haptics == nullptr || report_len < kReportLen || haptics_len < kHapticsPayloadLen) {
        return false;
    }

    uint8_t seq = 0;
    uint8_t packet = 0;
    bool headset = false;
    bool latest_speaker_valid = false;
    uint8_t speaker_copy[kSpeakerPayloadLen]{};
    snapshot_audio_state(&seq, &packet, &headset, speaker_copy, &latest_speaker_valid);

    if (speaker != nullptr && speaker_len > 0) {
        const auto copy_len = std::min(speaker_len, kSpeakerPayloadLen);
        std::memset(speaker_copy, 0, sizeof(speaker_copy));
        std::memcpy(speaker_copy, speaker, copy_len);
        latest_speaker_valid = true;
    }

    std::memset(report, 0, kReportLen);

    report[0] = kReportId;
    report[1] = seq << 4;
    report[2] = kAudioHeaderTag;
    report[3] = 7;
    report[4] = 0xfe;

    const uint8_t buffer_len = get_config().audio_buffer_length;
    report[5] = buffer_len;
    report[6] = buffer_len;
    report[7] = buffer_len;
    report[8] = buffer_len;
    report[9] = buffer_len;
    report[10] = packet;

    report[11] = kStateTag;
    report[12] = kOutputStateLen;
    state_set(report + 13, kOutputStateLen);

    report[76] = kHapticsTag;
    report[77] = kHapticsPayloadLen;
    std::memcpy(report + 78, haptics, kHapticsPayloadLen);

    report[142] = headset ? kHeadsetTag : kSpeakerTag;
    report[143] = kSpeakerPayloadLen;
    if (latest_speaker_valid) {
        std::memcpy(report + 144, speaker_copy, kSpeakerPayloadLen);
    }

    return true;
}

void audio_init() {
    bridge_set_speaker_active(false);

    if (s_audio_queue != nullptr) {
        return;
    }

    s_audio_queue = xQueueCreate(kAudioQueueDepth, sizeof(AudioQueueItem));
    if (s_audio_queue == nullptr) {
        ESP_LOGE(TAG, "failed to create audio scaffold queue");
        return;
    }

    const BaseType_t created = xTaskCreatePinnedToCore(
        audio_task,
        "audio",
        kAudioTaskStack,
        nullptr,
        4,
        &s_audio_task,
        tskNO_AFFINITY
    );
    if (created != pdPASS) {
        ESP_LOGE(TAG, "failed to create audio scaffold task");
        vQueueDelete(s_audio_queue);
        s_audio_queue = nullptr;
        return;
    }

    ESP_LOGI(TAG, "audio scaffold initialized; UAC capture and Opus encode disabled");
}
