#include "config.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>

namespace {

int failures = 0;

#define CHECK(condition)                                                          \
    do {                                                                          \
        if (!(condition)) {                                                       \
            std::cerr << __FILE__ << ':' << __LINE__ << ": CHECK failed: "      \
                      << #condition << '\n';                                      \
            ++failures;                                                           \
        }                                                                         \
    } while (false)

bool no_pins_valid(uint8_t) {
    return false;
}

bool only_pin_7_valid(const uint8_t pin) {
    return pin == 7;
}

bool bodies_equal(const Config_body &lhs, const Config_body &rhs) {
    return std::memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

void test_layout_and_defaults() {
    CHECK(sizeof(float) == 4);
    CHECK(sizeof(Config_body) == 22);
    CHECK(sizeof(Config) == 32);
    CHECK(CONFIG_VERSION == 5);
    CHECK(CONFIG_MAGIC == 0x66ccff00u);
    CHECK(CONFIG_BODY_SIZE == sizeof(Config_body));
    CHECK(CONFIG_RECORD_SIZE == sizeof(Config));

    CHECK(offsetof(Config_body, config_version) == 0);
    CHECK(offsetof(Config_body, haptics_gain) == 1);
    CHECK(offsetof(Config_body, speaker_volume) == 5);
    CHECK(offsetof(Config_body, headset_volume) == 6);
    CHECK(offsetof(Config_body, speaker_gain) == 7);
    CHECK(offsetof(Config_body, inactive_time) == 8);
    CHECK(offsetof(Config_body, disable_pico_led) == 9);
    CHECK(offsetof(Config_body, polling_rate_mode) == 10);
    CHECK(offsetof(Config_body, audio_buffer_length) == 11);
    CHECK(offsetof(Config_body, controller_mode) == 12);
    CHECK(offsetof(Config_body, enable_usb_sn) == 13);
    CHECK(offsetof(Config_body, ps_shortcut_enabled) == 14);
    CHECK(offsetof(Config_body, mic_select) == 15);
    CHECK(offsetof(Config_body, speaker_select) == 16);
    CHECK(offsetof(Config_body, enable_wake) == 17);
    CHECK(offsetof(Config_body, trigger_reduce) == 18);
    CHECK(offsetof(Config_body, lock_volume) == 19);
    CHECK(offsetof(Config_body, status_gpio_pin) == 20);
    CHECK(offsetof(Config_body, status_gpio_mode) == 21);

    CHECK(offsetof(Config, magic) == 0);
    CHECK(offsetof(Config, crc32) == 4);
    CHECK(offsetof(Config, size) == 8);
    CHECK(offsetof(Config, body) == 10);

    const auto body = config_default_body();
    CHECK(body.config_version == 5);
    CHECK(body.haptics_gain == 1.0f);
    CHECK(body.speaker_volume == 100);
    CHECK(body.headset_volume == 100);
    CHECK(body.speaker_gain == 2);
    CHECK(body.inactive_time == 30);
    CHECK(body.disable_pico_led == 0);
    CHECK(body.polling_rate_mode == 1);
    CHECK(body.audio_buffer_length == 48);
    CHECK(body.controller_mode == 2);
    CHECK(body.enable_usb_sn == 0);
    CHECK(body.ps_shortcut_enabled == 0);
    CHECK(body.mic_select == 0);
    CHECK(body.speaker_select == 0);
    CHECK(body.enable_wake == 0);
    CHECK(body.trigger_reduce == 0);
    CHECK(body.lock_volume == 0);
    CHECK(body.status_gpio_pin == 0xff);
    CHECK(body.status_gpio_mode == 0);

    constexpr uint8_t expected[] = {
        0x05, 0x00, 0x00, 0x80, 0x3f, 100, 100, 2, 30, 0, 1,
        48,   2,    0,    0,    0,    0,   0,   0, 0,  0xff, 0,
    };
    CHECK(sizeof(expected) == sizeof(body));
    CHECK(std::memcmp(&body, expected, sizeof(expected)) == 0);
    CHECK(config_body_crc(body) == 0x0e69e505u);
}

void test_matching_schema_normalization() {
    auto body = config_default_body();
    body.haptics_gain = 2.0f;
    body.speaker_volume = 0;
    body.headset_volume = 127;
    body.speaker_gain = 0;
    body.inactive_time = 0;
    body.disable_pico_led = 1;
    body.polling_rate_mode = 0;
    body.audio_buffer_length = 16;
    body.controller_mode = 0;
    body.enable_usb_sn = 1;
    body.ps_shortcut_enabled = 1;
    body.mic_select = 3;
    body.speaker_select = 3;
    body.enable_wake = 1;
    body.trigger_reduce = 10;
    body.lock_volume = 1;
    body.status_gpio_pin = 7;
    body.status_gpio_mode = 1;

    const auto lower = config_normalize_body(body, only_pin_7_valid);
    CHECK(lower.haptics_gain == 2.0f);
    CHECK(lower.speaker_volume == 0);
    CHECK(lower.headset_volume == 127);
    CHECK(lower.speaker_gain == 0);
    CHECK(lower.inactive_time == 0);
    CHECK(lower.disable_pico_led == 1);
    CHECK(lower.polling_rate_mode == 0);
    CHECK(lower.audio_buffer_length == 16);
    CHECK(lower.controller_mode == 0);
    CHECK(lower.enable_usb_sn == 1);
    CHECK(lower.ps_shortcut_enabled == 1);
    CHECK(lower.mic_select == 3);
    CHECK(lower.speaker_select == 3);
    CHECK(lower.enable_wake == 1);
    CHECK(lower.trigger_reduce == 10);
    CHECK(lower.lock_volume == 1);
    CHECK(lower.status_gpio_pin == 7);
    CHECK(lower.status_gpio_mode == 1);

    body.haptics_gain = 1.0f;
    body.speaker_volume = 127;
    body.headset_volume = 0;
    body.speaker_gain = 7;
    body.inactive_time = 60;
    body.polling_rate_mode = 2;
    body.audio_buffer_length = 128;
    body.controller_mode = 2;
    body.mic_select = 0;
    body.speaker_select = 0;
    body.trigger_reduce = 0;
    body.status_gpio_pin = 0xff;
    body.status_gpio_mode = 0;
    const auto upper = config_normalize_body(body, no_pins_valid);
    CHECK(upper.haptics_gain == 1.0f);
    CHECK(upper.speaker_volume == 127);
    CHECK(upper.headset_volume == 0);
    CHECK(upper.speaker_gain == 7);
    CHECK(upper.inactive_time == 60);
    CHECK(upper.polling_rate_mode == 2);
    CHECK(upper.audio_buffer_length == 128);
    CHECK(upper.controller_mode == 2);
    CHECK(upper.mic_select == 0);
    CHECK(upper.speaker_select == 0);
    CHECK(upper.trigger_reduce == 0);
    CHECK(upper.status_gpio_pin == 0xff);

    body = config_default_body();
    body.haptics_gain = std::numeric_limits<float>::quiet_NaN();
    body.speaker_volume = 128;
    body.headset_volume = 128;
    body.speaker_gain = 8;
    body.inactive_time = 61;
    body.disable_pico_led = 2;
    body.polling_rate_mode = 3;
    body.audio_buffer_length = 15;
    body.controller_mode = 3;
    body.enable_usb_sn = 2;
    body.ps_shortcut_enabled = 2;
    body.mic_select = 4;
    body.speaker_select = 4;
    body.enable_wake = 2;
    body.trigger_reduce = 11;
    body.lock_volume = 2;
    body.status_gpio_pin = 8;
    body.status_gpio_mode = 2;
    const auto invalid = config_normalize_body(body, only_pin_7_valid);
    CHECK(bodies_equal(invalid, config_default_body()));

    for (const float invalid_gain : {
             0.999f,
             2.001f,
             std::numeric_limits<float>::infinity(),
             -std::numeric_limits<float>::infinity(),
         }) {
        body = config_default_body();
        body.haptics_gain = invalid_gain;
        CHECK(config_normalize_body(body, no_pins_valid).haptics_gain == 1.0f);
    }

    body = config_default_body();
    body.audio_buffer_length = 129;
    CHECK(config_normalize_body(body, no_pins_valid).audio_buffer_length == 48);

    body = config_default_body();
    body.status_gpio_pin = 7;
    CHECK(config_normalize_body(body, nullptr).status_gpio_pin == 0xff);

    body = config_default_body();
    body.status_gpio_pin = 7;
    body.status_gpio_mode = 2;
    const auto invalid_mode = config_normalize_body(body, only_pin_7_valid);
    CHECK(invalid_mode.status_gpio_pin == 7);
    CHECK(invalid_mode.status_gpio_mode == 0);
}

void test_schema_mismatch_resets_every_field() {
    auto body = config_default_body();
    body.config_version = 4;
    body.haptics_gain = 2.0f;
    body.speaker_volume = 0;
    body.status_gpio_pin = 7;
    CHECK(bodies_equal(config_normalize_body(body, only_pin_7_valid),
                       config_default_body()));
}

void test_record_contract() {
    auto body = config_default_body();
    body.haptics_gain = 1.5f;
    body.speaker_volume = 0;
    body.inactive_time = 60;
    body.status_gpio_pin = 7;
    body.status_gpio_mode = 1;

    const Config first = config_make_record(body, only_pin_7_valid);
    const Config second = config_make_record(body, only_pin_7_valid);
    CHECK(first.magic == CONFIG_MAGIC);
    CHECK(first.size == CONFIG_BODY_SIZE);
    CHECK(first.crc32 == config_body_crc(first.body));
    CHECK(std::memcmp(&first, &second, sizeof(first)) == 0);

    Config_body decoded{};
    CHECK(config_decode_record(&first, sizeof(first), decoded, only_pin_7_valid) ==
          ConfigRecordStatus::valid);
    CHECK(bodies_equal(decoded, body));

    const Config_body sentinel = decoded;
    CHECK(config_decode_record(&first, sizeof(first) - 1, decoded, only_pin_7_valid) ==
          ConfigRecordStatus::invalid_length);
    CHECK(bodies_equal(decoded, sentinel));
    CHECK(config_decode_record(&first, 27, decoded, only_pin_7_valid) ==
          ConfigRecordStatus::invalid_length);
    CHECK(bodies_equal(decoded, sentinel));
    CHECK(config_decode_record(nullptr, sizeof(first), decoded, only_pin_7_valid) ==
          ConfigRecordStatus::invalid_length);
    CHECK(bodies_equal(decoded, sentinel));

    auto bad = first;
    bad.magic ^= 1u;
    CHECK(config_decode_record(&bad, sizeof(bad), decoded, only_pin_7_valid) ==
          ConfigRecordStatus::invalid_magic);
    CHECK(bodies_equal(decoded, sentinel));

    bad = first;
    bad.size = CONFIG_BODY_SIZE - 1;
    CHECK(config_decode_record(&bad, sizeof(bad), decoded, only_pin_7_valid) ==
          ConfigRecordStatus::invalid_size);
    CHECK(bodies_equal(decoded, sentinel));

    bad = first;
    bad.body.config_version = CONFIG_VERSION - 1;
    bad.crc32 = config_body_crc(bad.body);
    CHECK(config_decode_record(&bad, sizeof(bad), decoded, only_pin_7_valid) ==
          ConfigRecordStatus::incompatible_version);
    CHECK(bodies_equal(decoded, sentinel));

    bad = first;
    bad.crc32 ^= 1u;
    CHECK(config_decode_record(&bad, sizeof(bad), decoded, only_pin_7_valid) ==
          ConfigRecordStatus::invalid_crc);
    CHECK(bodies_equal(decoded, sentinel));

    bad = first;
    bad.body.audio_buffer_length = 15;
    bad.body.status_gpio_pin = 8;
    bad.crc32 = config_body_crc(bad.body);
    CHECK(config_decode_record(&bad, sizeof(bad), decoded, only_pin_7_valid) ==
          ConfigRecordStatus::valid);
    CHECK(decoded.audio_buffer_length == 48);
    CHECK(decoded.status_gpio_pin == 0xff);
    CHECK(decoded.haptics_gain == body.haptics_gain);
    CHECK(decoded.speaker_volume == body.speaker_volume);
}

}  // namespace

int main() {
    test_layout_and_defaults();
    test_matching_schema_normalization();
    test_schema_mismatch_resets_every_field();
    test_record_contract();

    if (failures != 0) {
        std::cerr << failures << " config contract check(s) failed\n";
        return 1;
    }
    std::cout << "config contract checks passed\n";
    return 0;
}
