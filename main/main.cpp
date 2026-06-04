// DS5Dongle-ESP — ESP32-S31 firmware entrypoint.
//
// Migrated from the Raspberry Pi Pico 2W build. The original sources live in:
//   - ../master worktree            (pristine Pico 2W reference)
//   - ./legacy-pico/src             (in-branch reference copy)
//
// The Pico build ran everything from a single main() super-loop
// (cyw43_arch_poll / tud_task / audio_loop / interrupt_loop). On ESP-IDF that
// dissolves into app_main() doing ordered subsystem bring-up, after which the
// work is driven by FreeRTOS tasks and Bluedroid/TinyUSB event callbacks — no
// busy loop. Each subsystem is ported into its own component under
// components/<name>/ and its init call is wired in here by its porting group.
// Port plan: docs/MIGRATION.md.

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "report_buffer.h"
#include "config.h"            // group 5 — NVS-backed Config
#include "usb.h"               // group 2 — esp_tinyusb DualSense device
#include "bt.h"                // group 3 — Bluedroid BR/EDR HID host
#include "bridge.h"            // group 4 — BT <-> USB HID report relay
// Subsystem headers are included as each component is ported:
//   #include "bridge.h"       // group 4 — BT <-> USB HID report relay
//   #include "audio.h"        // group 6 — Opus + resampler over BT
//   #include "battery_led.h"  // group 7 — low-battery indicator

static const char *TAG = "ds5dongle";

// Bring up NVS: backs the device Config (group 5) and the Bluedroid BR/EDR
// bond store. Reformat once if the partition is from an incompatible layout.
static void init_nvs(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS unusable (%s); erasing and re-initializing", esp_err_to_name(err));
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "DS5Dongle-ESP booting on ESP32-S31 (DualSense BT<->USB bridge)");

    // Persistent storage first — config and BT bonds depend on it.
    init_nvs();

    // Shared BT-input -> USB-input report buffer (portMUX-guarded; replaces the
    // Pico critical_section_t + interrupt_in_data global).
    report_buffer_init();

    // Subsystem bring-up in dependency order. Each call is added by its porting
    // group; until then the firmware boots, logs, and idles. Hang protection is
    // the IDF task watchdog (CONFIG_ESP_TASK_WDT_*, sdkconfig.defaults); the
    // report-bridge task subscribes to it via esp_task_wdt_add in group 4.
    config_load();                                      // group 5 (NVS)
    usb_init();                                          // group 2
    bt_init();                                           // group 3
    bridge_init();                                       // group 4 (registers on_bt_data + send task)
    //   audio_init();                                      // group 6
    //   battery_led_init();                                // group 7

    ESP_LOGI(TAG, "boot complete; subsystems pending port (see docs/MIGRATION.md)");

    // app_main returns: TinyUSB/Bluedroid/bridge run on their own FreeRTOS
    // tasks. No Pico super-loop remains.
}
