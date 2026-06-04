//
// Device configuration (ported from legacy-pico/src/config.cpp).
//
// The Pico build stored Config in the last flash sector (hardware/flash +
// hardware/sync). Here it persists as a single NVS blob (namespace "ds5", key
// "config"), keeping the same magic/version/size/CRC validation as a
// belt-and-braces check over NVS's own integrity. The disable_pico_led ->
// onboard-LED routing moves to the battery indicator (group 7 / task 5.4).
//

#include "config.h"

#include <cmath>
#include <cstring>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "utils.h"

static const char *TAG = "config";

constexpr uint32_t CONFIG_MAGIC = 0x66ccff00;
constexpr uint16_t CONFIG_VERSION = 1;
#define CONFIG_NVS_NS  "ds5"
#define CONFIG_NVS_KEY "config"

static Config config{};
bool is_dse = false;

static uint32_t calc_config_crc(const Config &con) {
    return crc32(reinterpret_cast<const uint8_t *>(&con.body), sizeof(Config_body));
}

void config_valid() {
    // Validate the header + every field, substituting the documented default
    // for anything out of range.
    if (config.magic != CONFIG_MAGIC) {
        config.magic = CONFIG_MAGIC;
        ESP_LOGW(TAG, "Config magic header invalid");
    }
    if (config.version != CONFIG_VERSION) {
        config.version = CONFIG_VERSION;
        ESP_LOGW(TAG, "Config version invalid");
    }
    if (config.size != sizeof(Config_body)) {
        config.size = sizeof(Config_body);
        ESP_LOGW(TAG, "Config body size invalid");
    }
    auto body = &config.body;
    if (std::isnan(body->haptics_gain) || body->haptics_gain < 1.0f || body->haptics_gain > 2.0f) {
        body->haptics_gain = 1.0f;
        ESP_LOGW(TAG, "Haptics gain invalid");
    }
    if (std::isnan(body->speaker_volume) || body->speaker_volume < -100 || body->speaker_volume > 0) {
        body->speaker_volume = -100;
        ESP_LOGW(TAG, "Speaker volume invalid");
    }
    if (body->inactive_time < 5 || body->inactive_time > 60) {
        body->inactive_time = 30;
        ESP_LOGW(TAG, "Inactive time invalid");
    }
    if (body->disable_inactive_disconnect > 1) {
        body->disable_inactive_disconnect = 0;
        ESP_LOGW(TAG, "disable_inactive_disconnect invalid");
    }
    if (body->disable_pico_led > 1) {
        body->disable_pico_led = 0;
        ESP_LOGW(TAG, "disable_pico_led invalid");
    }
    if (body->polling_rate_mode > 2) {
        body->polling_rate_mode = 0;
        ESP_LOGW(TAG, "polling_rate_mode invalid");
    }
    if (body->audio_buffer_length < 16 || body->audio_buffer_length > 128) {
        body->audio_buffer_length = 64;
        ESP_LOGW(TAG, "audio_buffer_length invalid");
    }
    if (body->controller_mode > 2) {
        body->controller_mode = 2;
        ESP_LOGW(TAG, "controller_mode invalid");
    }
    if (body->config_version != CONFIG_VERSION) {
        body->config_version = CONFIG_VERSION;
        ESP_LOGW(TAG, "Config may be a breaking change");
    }
}

void config_default() {
    memset(&config, 0, sizeof(config));
    config_valid(); // fills magic/version/size + all field defaults
}

void config_load() {
    bool loaded = false;
    nvs_handle_t h;
    if (nvs_open(CONFIG_NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        Config tmp{};
        size_t sz = sizeof(tmp);
        if (nvs_get_blob(h, CONFIG_NVS_KEY, &tmp, &sz) == ESP_OK && sz == sizeof(Config)) {
            config = tmp;
            loaded = true;
        }
        nvs_close(h);
    }
    if (!loaded) {
        ESP_LOGW(TAG, "No valid config in NVS, using defaults");
        memset(&config, 0, sizeof(config));
    }
    config_valid();
}

bool config_save() {
    config.crc32 = calc_config_crc(config);

    nvs_handle_t h;
    esp_err_t err = nvs_open(CONFIG_NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return false;
    }
    err = nvs_set_blob(h, CONFIG_NVS_KEY, &config, sizeof(Config));
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Config NVS write failed: %s", esp_err_to_name(err));
        return false;
    }

    // Read back and verify the persisted record (replaces the Pico flash verify).
    Config verify{};
    if (nvs_open(CONFIG_NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        size_t sz = sizeof(verify);
        esp_err_t rb = nvs_get_blob(h, CONFIG_NVS_KEY, &verify, &sz);
        nvs_close(h);
        if (rb == ESP_OK && sz == sizeof(verify) && calc_config_crc(verify) == config.crc32) {
            ESP_LOGI(TAG, "Config saved + verified");
            return true;
        }
    }
    ESP_LOGW(TAG, "Config save verify failed");
    return false;
}

const Config_body &get_config() {
    return config.body;
}

void set_config(const uint8_t *new_config, const uint16_t len) {
    const auto copy_len = len < sizeof(Config_body) ? len : sizeof(Config_body);
    memcpy(&config.body, new_config, copy_len);
    config_valid();
    // TODO(group 7 / task 5.4): route disable_pico_led to the battery indicator
    // (Pico called cyw43_arch_gpio_put here).
}

void set_config(const Config_body &new_config) {
    config.body = new_config;
    config_valid();
}
