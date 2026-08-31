#include "config.h"

#include <cmath>
#include <cstring>

#include "utils.h"

namespace {

constexpr bool is_bool(const uint8_t value) {
    return value <= 1;
}

}  // namespace

Config_body config_default_body() {
    Config_body defaults{};
    defaults.config_version = CONFIG_VERSION;
    defaults.haptics_gain = 1.0f;
    defaults.speaker_volume = 100;
    defaults.headset_volume = 100;
    defaults.speaker_gain = 2;
    defaults.inactive_time = 30;
    defaults.disable_pico_led = 0;
    defaults.polling_rate_mode = 1;
    defaults.audio_buffer_length = 48;
    defaults.controller_mode = 2;
    defaults.enable_usb_sn = 0;
    defaults.ps_shortcut_enabled = 0;
    defaults.mic_select = 0;
    defaults.speaker_select = 0;
    defaults.enable_wake = 0;
    defaults.trigger_reduce = 0;
    defaults.lock_volume = 0;
    defaults.status_gpio_pin = 0xff;
    defaults.status_gpio_mode = 0;
    return defaults;
}

Config_body config_normalize_body(const Config_body &body,
                                  const ConfigStatusPinPredicate status_pin_is_valid) {
    const Config_body defaults = config_default_body();
    if (body.config_version != CONFIG_VERSION) {
        return defaults;
    }

    Config_body normalized = body;
    if (!std::isfinite(normalized.haptics_gain) || normalized.haptics_gain < 1.0f ||
        normalized.haptics_gain > 2.0f) {
        normalized.haptics_gain = defaults.haptics_gain;
    }
    if (normalized.speaker_volume > 127) {
        normalized.speaker_volume = defaults.speaker_volume;
    }
    if (normalized.headset_volume > 127) {
        normalized.headset_volume = defaults.headset_volume;
    }
    if (normalized.speaker_gain > 7) {
        normalized.speaker_gain = defaults.speaker_gain;
    }
    if (normalized.inactive_time > 60) {
        normalized.inactive_time = defaults.inactive_time;
    }
    if (!is_bool(normalized.disable_pico_led)) {
        normalized.disable_pico_led = defaults.disable_pico_led;
    }
    if (normalized.polling_rate_mode > 2) {
        normalized.polling_rate_mode = defaults.polling_rate_mode;
    }
    if (normalized.audio_buffer_length < 16 || normalized.audio_buffer_length > 128) {
        normalized.audio_buffer_length = defaults.audio_buffer_length;
    }
    if (normalized.controller_mode > 2) {
        normalized.controller_mode = defaults.controller_mode;
    }
    if (!is_bool(normalized.enable_usb_sn)) {
        normalized.enable_usb_sn = defaults.enable_usb_sn;
    }
    if (!is_bool(normalized.ps_shortcut_enabled)) {
        normalized.ps_shortcut_enabled = defaults.ps_shortcut_enabled;
    }
    if (normalized.mic_select > 3) {
        normalized.mic_select = defaults.mic_select;
    }
    if (normalized.speaker_select > 3) {
        normalized.speaker_select = defaults.speaker_select;
    }
    if (!is_bool(normalized.enable_wake)) {
        normalized.enable_wake = defaults.enable_wake;
    }
    if (normalized.trigger_reduce > 10) {
        normalized.trigger_reduce = defaults.trigger_reduce;
    }
    if (!is_bool(normalized.lock_volume)) {
        normalized.lock_volume = defaults.lock_volume;
    }
    if (normalized.status_gpio_pin != 0xff &&
        (status_pin_is_valid == nullptr || !status_pin_is_valid(normalized.status_gpio_pin))) {
        normalized.status_gpio_pin = defaults.status_gpio_pin;
    }
    if (!is_bool(normalized.status_gpio_mode)) {
        normalized.status_gpio_mode = defaults.status_gpio_mode;
    }
    return normalized;
}

uint32_t config_body_crc(const Config_body &body) {
    return crc32(reinterpret_cast<const uint8_t *>(&body), sizeof(body));
}

Config config_make_record(const Config_body &body,
                          const ConfigStatusPinPredicate status_pin_is_valid) {
    Config record{};
    record.magic = CONFIG_MAGIC;
    record.size = CONFIG_BODY_SIZE;
    record.body = config_normalize_body(body, status_pin_is_valid);
    record.crc32 = config_body_crc(record.body);
    return record;
}

ConfigRecordStatus config_decode_record(const void *record_data,
                                        const size_t record_length,
                                        Config_body &decoded_body,
                                        const ConfigStatusPinPredicate status_pin_is_valid) {
    if (record_data == nullptr || record_length != CONFIG_RECORD_SIZE) {
        return ConfigRecordStatus::invalid_length;
    }

    Config record{};
    std::memcpy(&record, record_data, sizeof(record));
    if (record.magic != CONFIG_MAGIC) {
        return ConfigRecordStatus::invalid_magic;
    }
    if (record.size != CONFIG_BODY_SIZE) {
        return ConfigRecordStatus::invalid_size;
    }
    if (record.body.config_version != CONFIG_VERSION) {
        return ConfigRecordStatus::incompatible_version;
    }
    if (record.crc32 != config_body_crc(record.body)) {
        return ConfigRecordStatus::invalid_crc;
    }

    decoded_body = config_normalize_body(record.body, status_pin_is_valid);
    return ConfigRecordStatus::valid;
}
