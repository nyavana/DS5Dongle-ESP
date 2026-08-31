#include "cmd_contract.h"

#include <algorithm>
#include <cstring>

size_t cmd_copy_bounded(const uint8_t *source, size_t source_size,
                        uint8_t *destination, size_t destination_size) {
    if (source == nullptr || destination == nullptr) {
        return 0;
    }

    const size_t copy_size = std::min(source_size, destination_size);
    if (copy_size != 0) {
        std::memcpy(destination, source, copy_size);
    }
    return copy_size;
}

size_t cmd_build_status_response(int8_t rssi, uint8_t *destination,
                                 size_t destination_size) {
    if (destination == nullptr || destination_size == 0) {
        return 0;
    }

    destination[0] = static_cast<uint8_t>(rssi);
    if (destination_size == 1) {
        return 1;
    }

    destination[1] = 0x80;
    return 2;
}

CmdSetRequest cmd_parse_set_request(uint8_t report_id, const uint8_t *buffer,
                                    size_t buffer_size,
                                    size_t config_body_size) {
    constexpr uint8_t kSetReportId = 0xf6;
    constexpr uint8_t kUpdateConfig = 0x01;
    constexpr uint8_t kSaveConfig = 0x02;
    constexpr uint8_t kReconnectUsb = 0x03;
    const CmdSetRequest invalid{CmdSetAction::invalid, nullptr, 0};

    if (report_id != kSetReportId || buffer == nullptr || buffer_size == 0) {
        return invalid;
    }

    switch (buffer[0]) {
        case kUpdateConfig:
            if (config_body_size == 0 || buffer_size - 1 < config_body_size) {
                return invalid;
            }
            return {CmdSetAction::update, buffer + 1, config_body_size};
        case kSaveConfig:
            return {CmdSetAction::save, nullptr, 0};
        case kReconnectUsb:
            return {CmdSetAction::reconnect, nullptr, 0};
        default:
            return {CmdSetAction::unknown, nullptr, 0};
    }
}
