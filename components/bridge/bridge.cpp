//
// BT <-> USB HID report bridge (ported from the Pico build's main.cpp:
// interrupt_loop / on_bt_data / tud_hid_*_report_cb).
//

#include "bridge.h"

#include <cstring>
#include <vector>

#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tusb.h"

#include "bt.h"
#include "cmd.h"
#include "config.h"
#include "report_buffer.h"
#include "state_mgr.h"
#include "utils.h"

static const char *TAG = "bridge";

static int s_report_seq = 0;
static bool s_spk_active = false;

// Weak hooks filled by later groups so the bridge's spec behaviour (headset
// propagation, battery notify) links now and gains real bodies later:
//   set_headset            -> audio        (group 6)
//   battery_led_note_report -> battery_led  (group 7)
__attribute__((weak)) void set_headset(bool state) { (void) state; }
__attribute__((weak)) void battery_led_note_report(void) {}

void bridge_set_speaker_active(bool active) { s_spk_active = active; }

//--------------------------------------------------------------------+
// BT -> USB input relay
//--------------------------------------------------------------------+
void on_bt_data(CHANNEL_TYPE channel, uint8_t *data, uint16_t len) {
    (void) len;
    if (channel == INTERRUPT && data[1] == 0x31) {
        // Headset-presence bit change (new payload[53] vs stored payload[53]).
        if ((data[56] & 1) != (report_buffer_get_byte(53) & 1)) {
            set_headset(data[56] & 1);
        }
        // Direct modes copy without dirty gating; real-time mode (2) marks dirty.
        const bool realtime = (get_config().polling_rate_mode == 2);
        report_buffer_store(data + 3, realtime);
        battery_led_note_report();
    }
}

// Interrupt-report send loop — replaces the Pico interrupt_loop() that ran each
// super-loop iteration. Subscribes to the task watchdog (firmware-boot spec).
static void bridge_task(void *arg) {
    (void) arg;
    esp_task_wdt_add(nullptr);
    uint8_t buf[REPORT_IN_LEN];
    while (true) {
        esp_task_wdt_reset();
        if (tud_hid_ready()) {
            if (get_config().polling_rate_mode != 2) {
                report_buffer_get(buf);
                if (!tud_hid_report(0x01, buf, REPORT_IN_LEN)) {
                    ESP_LOGD(TAG, "tud_hid_report error");
                }
            } else if (report_buffer_get_if_dirty(buf)) {
                if (!tud_hid_report(0x01, buf, REPORT_IN_LEN)) {
                    report_buffer_set_dirty(); // re-arm so it retries next poll
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void bridge_init() {
    state_init();
    bt_register_data_callback(on_bt_data);
    xTaskCreatePinnedToCore(bridge_task, "bridge", 4096, nullptr, 5, nullptr, tskNO_AFFINITY);
    ESP_LOGI(TAG, "report bridge started");
}

//--------------------------------------------------------------------+
// USB HID class callbacks (TinyUSB links these by name; relocated here from
// usb.cpp so all bridging logic lives in one component)
//--------------------------------------------------------------------+

// GET_REPORT: command IDs -> cmd interface; other feature IDs -> stored data.
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type,
                               uint8_t *buffer, uint16_t reqlen) {
    (void) itf;
    (void) report_type;
    if (is_pico_cmd(report_id)) {
        return pico_cmd_get(report_id, buffer, reqlen);
    }
    std::vector<uint8_t> feature_data = get_feature_data(report_id, reqlen);
    if (!feature_data.empty()) {
        memcpy(buffer, feature_data.data() + 1, feature_data.size() - 1);
    }
    return feature_data.empty() ? 0 : feature_data.size() - 1;
}

// SET_REPORT / interrupt-OUT: command IDs -> cmd; report 0 byte 0x02 -> output
// state + BT 0x31; feature IDs -> feature store.
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t bufsize) {
    (void) itf;
    (void) report_type;
    if (is_pico_cmd(report_id)) {
        pico_cmd_set(report_id, buffer, bufsize);
        return;
    }

    // INTERRUPT OUT (report id 0)
    if (report_id == 0) {
        switch (buffer[0]) {
            case 0x02: {
                state_update(buffer + 1, bufsize - 1);
                if (s_spk_active) {
                    break; // output state rides in the audio 0x36 report instead
                }
                uint8_t outputData[78]{};
                outputData[0] = 0x31;
                outputData[1] = s_report_seq << 4;
                if (++s_report_seq == 256) {
                    s_report_seq = 0;
                }
                outputData[2] = 0x10;
                state_set(outputData + 3, sizeof(SetStateData));
                bt_write(outputData, sizeof(outputData));
                break;
            }
        }
    }

    if (report_id == 0x80 || report_id == 0x60 || report_id == 0x62 || report_id == 0x61) {
        set_feature_data(report_id, const_cast<uint8_t *>(buffer), bufsize);
    }
}
