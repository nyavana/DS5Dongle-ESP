//
// Custom feature-report command interface (ported from legacy-pico/src/cmd.cpp).
//
// Implements the 0xf6-0xf9 command protocol over HID feature reports. TinyUSB
// APIs stay available through esp_tinyusb, sleep_ms becomes vTaskDelay, and the
// firmware version string is build-defined.
//

#include "cmd.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "bt.h"
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tusb.h"

#ifndef DS5DONGLE_VERSION_STRING
#define DS5DONGLE_VERSION_STRING "1.0.0-esp32s31"
#endif

bool is_pico_cmd(uint8_t report_id) {
    return report_id == 0xf6 ||
           report_id == 0xf7 ||
           report_id == 0xf8 ||
           report_id == 0xf9;
}

uint16_t pico_cmd_get(uint8_t report_id, uint8_t *buffer, uint16_t reqlen) {
    if (report_id == 0xf7) {
        printf("[HID] Receive 0xf7 getting config\n");
        if (sizeof(Config_body) > reqlen) {
            printf("[Config] Warning: Config_body overflow\n");
        }
        const auto len = std::min(sizeof(Config_body), static_cast<size_t>(reqlen));
        memcpy(buffer, &get_config(), len);
        return len;
    }
    if (report_id == 0xf8) {
        printf("[HID] Receive 0xf8 getting firmware version\n");
        const auto len = std::min(strlen(DS5DONGLE_VERSION_STRING), static_cast<size_t>(reqlen));
        memcpy(buffer, DS5DONGLE_VERSION_STRING, len);
        return len;
    }
    if (report_id == 0xf9) {
        // [-128,0]
        int8_t rssi = 0;
        bt_get_signal_strength(&rssi);
        if (reqlen == 0) {
            return 0;
        }
        buffer[0] = rssi;
        return 1;
    }
    return 0;
}

void pico_cmd_set(uint8_t report_id, uint8_t const *buffer, uint16_t bufsize) {
    (void) report_id;
    if (bufsize == 0) {
        return;
    }

    // 0x01 update config in variable
    // 0x02 write config to NVS
    // 0x03 reconnect tinyusb device
    if (buffer[0] == 0x01) {
        printf("[CMD] Enter config set func\n");
        set_config(buffer + 1, bufsize - 1);
    }
    if (buffer[0] == 0x02) {
        printf("[CMD] Enter config save func\n");
        config_save();
    }
    if (buffer[0] == 0x03) {
        printf("[CMD] Enter tud reconnect func\n");
        tud_disconnect();
        vTaskDelay(pdMS_TO_TICKS(150));
        tud_connect();
    }
}
