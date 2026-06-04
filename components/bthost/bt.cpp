//
// Bluetooth BR/EDR HID host (ported from legacy-pico/src/bt.cpp).
//
// The Pico build was a hand-rolled HID host over raw BTstack L2CAP (manual
// inquiry + class-of-device filter, SSP pairing, explicit PSM 0x11/0x13
// channels, hand-built 0xA2/0xA3/0x43/0x53 transaction framing). On ESP-IDF
// this is re-implemented on the Bluedroid **esp_hidh** BR/EDR HID host (design
// D1): esp_hidh manages SDP, the HID control/interrupt L2CAP channels, pairing,
// bonding (NVS) and reconnection, and exposes input/feature reports as events.
//
// esp_hidh answers the design's open question: it DOES pass raw DualSense
// reports (INPUT event = report_id + raw data) and supports class-of-device-
// filtered discovery (esp_bt_gap inquiry), so the raw-L2CAP fallback (task 3.5)
// is NOT needed. The unchanged bt.h contract is preserved by a thin adapter:
// the INPUT event is reframed into the raw [0xA1][0x31][...] buffer the bridge
// expects, bt_write maps to esp_hidh_dev_output_set (the DualSense CRC seed
// already accounts for the 0xA2 header esp_hidh prepends), and feature get/set
// map to esp_hidh GET/SET_REPORT with the FEATURE event populating the cache.
//
// Deferred to later groups (kept out so group 3 builds standalone):
//   - the initial 0x32 report body via state_set            (group 4)
//   - inactivity disconnect + LED, both config-driven        (groups 5/7)
//   - is_dse -> device-config + USB reconnect on DS/DSE detect(group 5)
//

#include "bt.h"

#include <cstring>
#include <unordered_map>
#include <vector>

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_gap_bt_api.h"
#include "esp_hid_common.h"
#include "esp_hidh.h"
#include "esp_hidh_bluedroid.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "utils.h"

static const char *TAG = "bt";

// Gamepad class-of-device mask: Major device class == Peripheral/Gamepad.
#define COD_GAMEPAD_MASK  0x000F00
#define COD_GAMEPAD_VALUE 0x000500

static bt_data_callback_t s_cb = nullptr;
static esp_hidh_dev_t *s_dev = nullptr;
static esp_bd_addr_t s_addr = {0};
static bool s_connected = false;
static size_t s_map_index = 0;
static int8_t s_rssi = 0;
static bool s_check_dse = false;
static bool s_is_dse = false;  // TODO(group 5): mirror into device-config is_dse

static std::unordered_map<uint8_t, std::vector<uint8_t>> s_feature;
static SemaphoreHandle_t s_feature_mtx = nullptr;

static inline void feature_lock() { if (s_feature_mtx) xSemaphoreTake(s_feature_mtx, portMAX_DELAY); }
static inline void feature_unlock() { if (s_feature_mtx) xSemaphoreGive(s_feature_mtx); }

static void start_discovery() {
    // inq_len in 1.28s units (30 ~= 38s), like the Pico gap_inquiry_start(30).
    esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 30, 0);
}

void bt_register_data_callback(bt_data_callback_t callback) {
    s_cb = callback;
}

// Send the controller-init 0x32 report on connect. Group 4 fills the body via
// state_set; for now the SetStateData body stays zeroed.
static void send_initial_report() {
    uint8_t report32[142] = {};
    report32[0] = 0x32;
    report32[1] = 0x10;                  // reportSeqCounter
    report32[2] = 0x10 | (0 << 6) | (1 << 7);
    report32[3] = 0x3f;                  // 63 bytes
    // TODO(group 4): state_set(report32 + 4, sizeof(SetStateData));
    bt_write(report32, sizeof(report32));
}

//--------------------------------------------------------------------+
// esp_hidh event handler
//--------------------------------------------------------------------+
static void hidh_callback(void *handler_args, esp_event_base_t base, int32_t id, void *event_data) {
    (void) handler_args;
    (void) base;
    esp_hidh_event_t event = (esp_hidh_event_t) id;
    esp_hidh_event_data_t *param = (esp_hidh_event_data_t *) event_data;

    switch (event) {
        case ESP_HIDH_OPEN_EVENT: {
            if (param->open.status == ESP_OK) {
                s_dev = param->open.dev;
                s_connected = true;
                s_rssi = 0;
                ESP_LOGI(TAG, "HID device opened");
                init_feature();
                send_initial_report();
                esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
            } else {
                ESP_LOGW(TAG, "HID open failed, restart discovery");
                start_discovery();
            }
            break;
        }

        case ESP_HIDH_INPUT_EVENT: {
            if (param->input.report_id == 0x31 && s_cb) {
                // Reframe to the raw SDU the bridge expects: [0xA1][0x31][data...]
                // (the esp_hidh INPUT data starts at the byte after the report id,
                // matching the Pico's data+2; the bridge reads data+3 / data[56]).
                static uint8_t recon[512];
                uint16_t n = param->input.length;
                if (n > sizeof(recon) - 2) n = sizeof(recon) - 2;
                recon[0] = 0xA1;
                recon[1] = 0x31;
                memcpy(recon + 2, param->input.data, n);
                s_map_index = param->input.map_index;
                s_cb(INTERRUPT, recon, n + 2);
                // TODO(group 5): inactivity disconnect; (group 7): battery_led_note_report().
            }
            break;
        }

        case ESP_HIDH_FEATURE_EVENT: {
            uint8_t rid = (uint8_t) param->feature.report_id;
            feature_lock();
            s_feature[rid].assign(param->feature.data, param->feature.data + param->feature.length);
            feature_unlock();
            // DSE detection: the Pico requests feature 0x70 — DSE returns data,
            // DS5 returns a single byte.
            if (s_check_dse && rid == 0x70) {
                s_check_dse = false;
                s_is_dse = (param->feature.length > 1);
                ESP_LOGI(TAG, "Detected %s controller", s_is_dse ? "DualSense Edge" : "DualSense");
                // TODO(group 5): set device-config is_dse + tud_connect() to re-enumerate USB.
            }
            break;
        }

        case ESP_HIDH_CLOSE_EVENT: {
            ESP_LOGI(TAG, "HID device closed, restart discovery");
            s_connected = false;
            s_dev = nullptr;
            s_rssi = 0;
            feature_lock();
            s_feature.clear();
            feature_unlock();
            if (param->close.dev) {
                esp_hidh_dev_free(param->close.dev);
            }
            // TODO(group 5): tud_disconnect(); (group 7): battery_led_on_disconnect().
            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
            start_discovery();
            break;
        }

        default:
            break;
    }
}

//--------------------------------------------------------------------+
// Classic GAP callback: discovery (CoD filter) + SSP pairing + RSSI
//--------------------------------------------------------------------+
static void gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
    switch (event) {
        case ESP_BT_GAP_DISC_RES_EVT: {
            uint32_t cod = 0;
            for (int i = 0; i < param->disc_res.num_prop; i++) {
                if (param->disc_res.prop[i].type == ESP_BT_GAP_DEV_PROP_COD) {
                    cod = *(uint32_t *) param->disc_res.prop[i].val;
                }
            }
            if ((cod & COD_GAMEPAD_MASK) == COD_GAMEPAD_VALUE) {
                ESP_LOGI(TAG, "Gamepad found (CoD 0x%06x), connecting", (unsigned) cod);
                memcpy(s_addr, param->disc_res.bda, sizeof(esp_bd_addr_t));
                esp_bt_gap_cancel_discovery();
                esp_hidh_dev_open(s_addr, ESP_HID_TRANSPORT_BT, 0);
            }
            break;
        }

        case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
            break;

        case ESP_BT_GAP_CFM_REQ_EVT:
            ESP_LOGI(TAG, "SSP user confirmation, accept (val %u)", (unsigned) param->cfm_req.num_val);
            esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
            break;

        case ESP_BT_GAP_KEY_NOTIF_EVT:
            ESP_LOGI(TAG, "SSP passkey: %u", (unsigned) param->key_notif.passkey);
            break;

        case ESP_BT_GAP_PIN_REQ_EVT: {
            // Legacy pairing fallback — reply "0000" like the Pico build.
            esp_bt_pin_code_t pin = {'0', '0', '0', '0'};
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin);
            break;
        }

        case ESP_BT_GAP_AUTH_CMPL_EVT:
            if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "Authentication success");
            } else {
                ESP_LOGW(TAG, "Authentication failed (0x%x), restart discovery", param->auth_cmpl.stat);
                start_discovery();
            }
            break;

        case ESP_BT_GAP_READ_RSSI_DELTA_EVT:
            if (param->read_rssi_delta.stat == ESP_BT_STATUS_SUCCESS) {
                s_rssi = param->read_rssi_delta.rssi_delta;
            }
            break;

        default:
            break;
    }
}

//--------------------------------------------------------------------+
// Public API
//--------------------------------------------------------------------+
int bt_init() {
    s_feature_mtx = xSemaphoreCreateMutex();

    // Default event loop (esp_hidh posts events to it). Ignore "already created".
    esp_err_t le = esp_event_loop_create_default();
    if (le != ESP_OK && le != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(le);
    }

    // Controller + Bluedroid in Classic (BR/EDR) mode. The S31
    // BT_CONTROLLER_INIT_CONFIG_DEFAULT() macro only sets a subset of fields, so
    // suppress -Wmissing-field-initializers (the rest are zero-initialized).
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
#pragma GCC diagnostic pop
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_ERROR_CHECK(esp_bt_gap_register_callback(gap_callback));

    // SSP: DisplayYesNo, auto-accept confirmations (the DualSense uses Just Works).
    esp_bt_io_cap_t io_cap = ESP_BT_IO_CAP_IO;
    ESP_ERROR_CHECK(esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &io_cap, sizeof(io_cap)));

    esp_bt_gap_set_device_name("DS5Dongle");
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

    esp_hidh_config_t hidh_cfg = {};
    hidh_cfg.callback = hidh_callback;
    hidh_cfg.event_stack_size = 4096;
    hidh_cfg.callback_arg = nullptr;
    ESP_ERROR_CHECK(esp_hidh_init(&hidh_cfg));

    start_discovery();
    ESP_LOGI(TAG, "BR/EDR HID host (esp_hidh) started, scanning for DualSense");
    return 0;
}

void bt_send_packet(uint8_t *data, uint16_t len) {
    // Raw interrupt-channel send is not exposed by esp_hidh; the bridge/audio
    // use bt_write (output reports). Kept for the bt.h contract.
    (void) data;
    (void) len;
}

void bt_send_control(uint8_t *data, uint16_t len) {
    (void) data;
    (void) len;
}

void bt_write(const uint8_t *data, const uint16_t len) {
    if (!s_connected || s_dev == nullptr || len < 5) return;
    static uint8_t buf[512];
    if (len > sizeof(buf)) return;
    memcpy(buf, data, len);
    // CRC over {0xA2(seed), report_id, body}; esp_hidh prepends the 0xA2 header.
    fill_output_report_checksum(buf, len);
    esp_hidh_dev_output_set(s_dev, s_map_index, buf[0], buf + 1, len - 1);
}

void bt_get_signal_strength(int8_t *rssi) {
    if (rssi != nullptr) {
        *rssi = s_rssi;
    }
    if (s_connected) {
        esp_bt_gap_read_rssi_delta(s_addr); // async; updates s_rssi on the next event
    }
}

std::vector<uint8_t> get_feature_data(uint8_t reportId, uint16_t len) {
    std::vector<uint8_t> ret;
    feature_lock();
    auto it = s_feature.find(reportId);
    if (it != s_feature.end()) {
        ret = it->second;
    }
    feature_unlock();

    // 0x81 always re-requests; DSE save reports (0x63/0x64/0x65) too; otherwise
    // request only when nothing cached yet.
    const bool need_request = ret.empty() ||
                              reportId == 0x81 ||
                              reportId == 0x63 ||
                              reportId == 0x65 ||
                              reportId == 0x64;
    if (need_request && s_connected && s_dev != nullptr) {
        esp_hidh_dev_get_report(s_dev, s_map_index, reportId, ESP_HID_REPORT_TYPE_FEATURE,
                                len ? len : 64);
    }
    return ret;
}

void set_feature_data(uint8_t reportId, uint8_t *data, uint16_t len) {
    if (!s_connected || s_dev == nullptr) return;
    static uint8_t buf[256];
    if ((size_t) len + 1 + 4 > sizeof(buf)) return;
    buf[0] = reportId;
    memcpy(buf + 1, data, len);
    // CRC over {0x53(seed), report_id, data}; esp_hidh prepends the 0x53 header.
    fill_feature_report_checksum(buf, len + 1 + 4);
    esp_hidh_dev_set_report(s_dev, s_map_index, reportId, ESP_HID_REPORT_TYPE_FEATURE,
                            buf + 1, len + 4);
}

void init_feature() {
    get_feature_data(0x09, 20);
    get_feature_data(0x20, 64);
    get_feature_data(0x22, 64);
    get_feature_data(0x05, 41);
    // Detect DS vs DSE by requesting feature 0x70.
    s_check_dse = true;
    get_feature_data(0x70, 64);
}
