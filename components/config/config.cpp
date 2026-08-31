//
// NVS adapter for the canonical version-5 configuration contract.
//

#include "config.h"

#include <cstring>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"

namespace {

constexpr char CONFIG_NVS_NS[] = "ds5";
constexpr char CONFIG_NVS_KEY[] = "config";

const char *TAG = "config";
Config_body current_body = config_default_body();

bool runtime_status_pin_is_valid(const uint8_t pin) {
    return GPIO_IS_VALID_OUTPUT_GPIO(static_cast<gpio_num_t>(pin));
}

const char *record_status_name(const ConfigRecordStatus status) {
    switch (status) {
        case ConfigRecordStatus::valid:
            return "valid";
        case ConfigRecordStatus::invalid_length:
            return "invalid length";
        case ConfigRecordStatus::invalid_magic:
            return "invalid magic";
        case ConfigRecordStatus::invalid_size:
            return "invalid body size";
        case ConfigRecordStatus::incompatible_version:
            return "incompatible schema version";
        case ConfigRecordStatus::invalid_crc:
            return "invalid CRC";
    }
    return "unknown";
}

bool read_exact_record(const nvs_handle_t handle, Config &record, esp_err_t &error) {
    size_t stored_size = 0;
    error = nvs_get_blob(handle, CONFIG_NVS_KEY, nullptr, &stored_size);
    if (error != ESP_OK || stored_size != CONFIG_RECORD_SIZE) {
        return false;
    }

    size_t read_size = sizeof(record);
    error = nvs_get_blob(handle, CONFIG_NVS_KEY, &record, &read_size);
    return error == ESP_OK && read_size == CONFIG_RECORD_SIZE;
}

}  // namespace

bool is_dse = false;

void config_default() {
    current_body = config_default_body();
}

void config_valid() {
    current_body = config_normalize_body(current_body, runtime_status_pin_is_valid);
}

void config_load() {
    nvs_handle_t handle;
    esp_err_t error = nvs_open(CONFIG_NVS_NS, NVS_READONLY, &handle);
    if (error != ESP_OK) {
        config_default();
        if (error == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Config is missing from NVS; using version-5 defaults");
        } else {
            ESP_LOGW(TAG, "Unable to open config NVS (%s); using version-5 defaults",
                     esp_err_to_name(error));
        }
        return;
    }

    Config stored{};
    size_t stored_size = 0;
    error = nvs_get_blob(handle, CONFIG_NVS_KEY, nullptr, &stored_size);
    if (error == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        config_default();
        ESP_LOGW(TAG, "Config is missing from NVS; using version-5 defaults");
        return;
    }
    if (error != ESP_OK) {
        nvs_close(handle);
        config_default();
        ESP_LOGW(TAG, "Config NVS size read failed (%s); using version-5 defaults",
                 esp_err_to_name(error));
        return;
    }
    if (stored_size != CONFIG_RECORD_SIZE) {
        nvs_close(handle);
        config_default();
        ESP_LOGW(TAG, "Incompatible config record length %u; using version-5 defaults",
                 static_cast<unsigned>(stored_size));
        return;
    }

    size_t read_size = sizeof(stored);
    error = nvs_get_blob(handle, CONFIG_NVS_KEY, &stored, &read_size);
    nvs_close(handle);
    if (error != ESP_OK || read_size != CONFIG_RECORD_SIZE) {
        config_default();
        if (error != ESP_OK) {
            ESP_LOGW(TAG, "Config NVS read failed (%s); using version-5 defaults",
                     esp_err_to_name(error));
        } else {
            ESP_LOGW(TAG, "Config NVS read length changed; using version-5 defaults");
        }
        return;
    }

    Config_body decoded{};
    const ConfigRecordStatus status =
        config_decode_record(&stored, sizeof(stored), decoded, runtime_status_pin_is_valid);
    if (status != ConfigRecordStatus::valid) {
        config_default();
        ESP_LOGW(TAG, "Rejected %s config record; using version-5 defaults",
                 record_status_name(status));
        return;
    }

    current_body = decoded;
    if (std::memcmp(&stored.body, &decoded, sizeof(decoded)) != 0) {
        ESP_LOGW(TAG, "Loaded config contained invalid fields; canonical defaults applied");
    } else {
        ESP_LOGI(TAG, "Loaded verified version-5 config from NVS");
    }
}

bool config_save() {
    current_body = config_normalize_body(current_body, runtime_status_pin_is_valid);
    const Config record = config_make_record(current_body, runtime_status_pin_is_valid);

    nvs_handle_t handle;
    esp_err_t error = nvs_open(CONFIG_NVS_NS, NVS_READWRITE, &handle);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Config NVS open for write failed: %s", esp_err_to_name(error));
        return false;
    }

    error = nvs_set_blob(handle, CONFIG_NVS_KEY, &record, sizeof(record));
    if (error != ESP_OK) {
        nvs_close(handle);
        ESP_LOGE(TAG, "Config NVS write failed: %s", esp_err_to_name(error));
        return false;
    }

    error = nvs_commit(handle);
    nvs_close(handle);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Config NVS commit failed: %s", esp_err_to_name(error));
        return false;
    }

    error = nvs_open(CONFIG_NVS_NS, NVS_READONLY, &handle);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Config NVS verification open failed: %s", esp_err_to_name(error));
        return false;
    }

    Config verified{};
    if (!read_exact_record(handle, verified, error)) {
        nvs_close(handle);
        if (error == ESP_OK) {
            ESP_LOGE(TAG, "Config NVS verification length mismatch");
        } else {
            ESP_LOGE(TAG, "Config NVS verification read failed: %s", esp_err_to_name(error));
        }
        return false;
    }
    nvs_close(handle);

    Config_body decoded{};
    const ConfigRecordStatus status =
        config_decode_record(&verified, sizeof(verified), decoded, runtime_status_pin_is_valid);
    if (status != ConfigRecordStatus::valid) {
        ESP_LOGE(TAG, "Config NVS verification rejected %s record", record_status_name(status));
        return false;
    }
    if (std::memcmp(&verified, &record, sizeof(record)) != 0 ||
        std::memcmp(&decoded, &current_body, sizeof(decoded)) != 0) {
        ESP_LOGE(TAG, "Config NVS verification body mismatch");
        return false;
    }

    ESP_LOGI(TAG, "Config saved to NVS and verified");
    return true;
}

const Config_body &get_config() {
    return current_body;
}

bool set_config(const uint8_t *new_config, const size_t len) {
    if (new_config == nullptr || len < sizeof(Config_body)) {
        return false;
    }

    Config_body replacement{};
    std::memcpy(&replacement, new_config, sizeof(replacement));
    current_body = config_normalize_body(replacement, runtime_status_pin_is_valid);
    return true;
}

void set_config(const Config_body &new_config) {
    current_body = config_normalize_body(new_config, runtime_status_pin_is_valid);
}
