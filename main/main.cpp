// DS5Dongle-ESP — ESP32-S31 firmware entrypoint.
//
// Migrated from the Raspberry Pi Pico 2W build. The original sources live in:
//   - ../master worktree            (pristine Pico 2W reference)
//   - ./legacy-pico/src             (in-branch reference copy)
//
// This is a scaffold. The Pico build ran everything from a single main() loop
// (bt poll / tud_task / audio / interrupt report). On ESP-IDF, wire each
// subsystem up here and drive it from FreeRTOS tasks + BT/USB event callbacks.
// Port plan: docs/MIGRATION.md.

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ds5dongle";

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "DS5Dongle-ESP booting on ESP32-S31");

    // --- Migration TODO (port from legacy-pico/src — see docs/MIGRATION.md) ---
    // config_load();   // config.cpp   -> NVS
    // bt_init();       // bt.cpp        -> Bluedroid Classic (BR/EDR) HID host  [DualSense link]
    // usb_init();      // usb.cpp + usb_descriptors.cpp -> esp_tinyusb device
    // audio_init();    // audio.cpp     -> Opus + I2S (S31 HW BT-audio sync)
    // state_init();    // state_mgr.cpp

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
