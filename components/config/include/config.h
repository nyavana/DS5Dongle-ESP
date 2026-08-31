//
// Canonical version-5 device configuration contract.
//
#ifndef DS5_BRIDGE_CONFIG_H
#define DS5_BRIDGE_CONFIG_H

#include <cstddef>
#include <cstdint>

inline constexpr uint32_t CONFIG_MAGIC = 0x66ccff00u;
inline constexpr uint8_t CONFIG_VERSION = 5;
inline constexpr uint16_t CONFIG_BODY_SIZE = 22;
inline constexpr size_t CONFIG_RECORD_SIZE = 32;

struct __attribute__((packed)) Config_body {
    uint8_t config_version;
    float haptics_gain;              // finite [1.0, 2.0]
    uint8_t speaker_volume;          // [0, 127]
    uint8_t headset_volume;          // [0, 127]
    uint8_t speaker_gain;            // [0, 7]
    uint8_t inactive_time;           // [0, 60] min
    uint8_t disable_pico_led;        // bool
    uint8_t polling_rate_mode;       // 0: 250 Hz, 1: 500 Hz, 2: real-time
    uint8_t audio_buffer_length;     // [16, 128]
    uint8_t controller_mode;         // 0: DS5, 1: DSE, 2: auto
    uint8_t enable_usb_sn;           // bool
    uint8_t ps_shortcut_enabled;     // bool
    uint8_t mic_select;              // [0, 3]
    uint8_t speaker_select;          // [0, 3]
    uint8_t enable_wake;             // bool
    uint8_t trigger_reduce;          // [0, 10]
    uint8_t lock_volume;             // bool
    uint8_t status_gpio_pin;         // output-capable SoC pin or 0xff disabled
    uint8_t status_gpio_mode;        // bool
};

struct __attribute__((packed)) Config {
    uint32_t magic;
    uint32_t crc32;
    uint16_t size;
    Config_body body;
};

static_assert(sizeof(float) == 4, "Config contract requires IEEE-754 float32");
static_assert(offsetof(Config_body, config_version) == 0);
static_assert(offsetof(Config_body, haptics_gain) == 1);
static_assert(offsetof(Config_body, speaker_volume) == 5);
static_assert(offsetof(Config_body, headset_volume) == 6);
static_assert(offsetof(Config_body, speaker_gain) == 7);
static_assert(offsetof(Config_body, inactive_time) == 8);
static_assert(offsetof(Config_body, disable_pico_led) == 9);
static_assert(offsetof(Config_body, polling_rate_mode) == 10);
static_assert(offsetof(Config_body, audio_buffer_length) == 11);
static_assert(offsetof(Config_body, controller_mode) == 12);
static_assert(offsetof(Config_body, enable_usb_sn) == 13);
static_assert(offsetof(Config_body, ps_shortcut_enabled) == 14);
static_assert(offsetof(Config_body, mic_select) == 15);
static_assert(offsetof(Config_body, speaker_select) == 16);
static_assert(offsetof(Config_body, enable_wake) == 17);
static_assert(offsetof(Config_body, trigger_reduce) == 18);
static_assert(offsetof(Config_body, lock_volume) == 19);
static_assert(offsetof(Config_body, status_gpio_pin) == 20);
static_assert(offsetof(Config_body, status_gpio_mode) == 21);
static_assert(sizeof(Config_body) == CONFIG_BODY_SIZE);

static_assert(offsetof(Config, magic) == 0);
static_assert(offsetof(Config, crc32) == 4);
static_assert(offsetof(Config, size) == 8);
static_assert(offsetof(Config, body) == 10);
static_assert(sizeof(Config) == CONFIG_RECORD_SIZE);

using ConfigStatusPinPredicate = bool (*)(uint8_t pin);

enum class ConfigRecordStatus : uint8_t {
    valid,
    invalid_length,
    invalid_magic,
    invalid_size,
    incompatible_version,
    invalid_crc,
};

// ESP-IDF-independent contract helpers used by firmware and host tests.
Config_body config_default_body();
Config_body config_normalize_body(const Config_body &body,
                                  ConfigStatusPinPredicate status_pin_is_valid);
uint32_t config_body_crc(const Config_body &body);
Config config_make_record(const Config_body &body,
                          ConfigStatusPinPredicate status_pin_is_valid);
ConfigRecordStatus config_decode_record(const void *record_data,
                                        size_t record_length,
                                        Config_body &decoded_body,
                                        ConfigStatusPinPredicate status_pin_is_valid);

// ESP-IDF runtime adapter.
void config_default();
void config_load();
bool config_save();
const Config_body &get_config();
bool set_config(const uint8_t *new_config, size_t len);
void set_config(const Config_body &new_config);
void config_valid();
extern bool is_dse;

#endif  // DS5_BRIDGE_CONFIG_H
