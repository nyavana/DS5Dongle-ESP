#pragma once

#include <cstddef>
#include <cstdint>

enum class CmdSetAction {
    invalid,
    update,
    save,
    reconnect,
    unknown,
};

struct CmdSetRequest {
    CmdSetAction action;
    const uint8_t *payload;
    size_t payload_size;
};

// Copy at most destination_size bytes from source. The return value is the
// number of bytes copied; invalid pointers never produce a partial response.
size_t cmd_copy_bounded(const uint8_t *source, size_t source_size,
                        uint8_t *destination, size_t destination_size);

// Build the 0xf9 body: RSSI followed, when requested, by the validity marker.
// Runtime audio is disabled, so the activity bits remain clear.
size_t cmd_build_status_response(int8_t rssi, uint8_t *destination,
                                 size_t destination_size);

// Decode a SET feature report without performing side effects. Only report
// 0xf6 is accepted, and update payloads must contain one complete config body.
CmdSetRequest cmd_parse_set_request(uint8_t report_id, const uint8_t *buffer,
                                    size_t buffer_size,
                                    size_t config_body_size);
