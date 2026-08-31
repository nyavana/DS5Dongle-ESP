//
// Custom feature-report command interface (ported from legacy-pico/src/cmd.cpp).
//
// Implements the 0xf6-0xf9 command protocol over HID feature reports. Buffer
// rules are kept in the host-testable command contract, and hardware side
// effects stay behind their owning components.
//

#include "cmd.h"

#include <cstdio>
#include <cstring>

#include "battery_led.h"
#include "bt.h"
#include "cmd_contract.h"
#include "config.h"
#include "usb.h"

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
        const auto *config = reinterpret_cast<const uint8_t *>(&get_config());
        return static_cast<uint16_t>(
            cmd_copy_bounded(config, sizeof(Config_body), buffer, reqlen));
    }
    if (report_id == 0xf8) {
        printf("[HID] Receive 0xf8 getting firmware version\n");
        const auto *version =
            reinterpret_cast<const uint8_t *>(DS5DONGLE_VERSION_STRING);
        return static_cast<uint16_t>(cmd_copy_bounded(
            version, strlen(DS5DONGLE_VERSION_STRING), buffer, reqlen));
    }
    if (report_id == 0xf9) {
        int8_t rssi = 0;
        bt_get_signal_strength(&rssi);
        return static_cast<uint16_t>(
            cmd_build_status_response(rssi, buffer, reqlen));
    }
    return 0;
}

void pico_cmd_set(uint8_t report_id, uint8_t const *buffer, uint16_t bufsize) {
    const CmdSetRequest request = cmd_parse_set_request(
        report_id, buffer, bufsize, sizeof(Config_body));

    switch (request.action) {
        case CmdSetAction::update:
            printf("[CMD] Enter config set func\n");
            if (set_config(request.payload, request.payload_size)) {
                battery_led_apply_config();
            }
            break;
        case CmdSetAction::save:
            printf("[CMD] Enter config save func\n");
            config_save();
            break;
        case CmdSetAction::reconnect:
            printf("[CMD] Enter USB reconnect func\n");
            usb_reconnect();
            break;
        case CmdSetAction::invalid:
        case CmdSetAction::unknown:
            break;
    }
}
