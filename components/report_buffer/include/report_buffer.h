//
// Shared DualSense input-report buffer.
//
// On the Pico build this was a bare `uint8_t interrupt_in_data[63]` global plus
// a `report_dirty` flag, guarded by a `critical_section_t`. The BT input
// callback wrote it and the USB super-loop read it. On ESP-IDF those run on
// different FreeRTOS tasks, so the buffer + dirty flag move behind a
// portMUX_TYPE critical section (firmware-boot spec: "Concurrency primitives").
//
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Length of the DualSense input-report payload bridged from the BT 0x31 report
// (data+3) to the USB HID 0x01 report.
#define REPORT_IN_LEN 63

// Initialize the shared buffer (DualSense default payload) and its spinlock.
// Call once from app_main() before any producer/consumer runs.
void report_buffer_init(void);

// Store a fresh REPORT_IN_LEN-byte payload under the lock. When `mark_dirty` is
// true (real-time polling mode) the dirty flag is raised so the next USB poll
// forwards this report exactly once.
void report_buffer_store(const uint8_t *payload, bool mark_dirty);

// Copy the current payload into `out` (>= REPORT_IN_LEN bytes) under the lock
// (direct 250/500 Hz polling modes).
void report_buffer_get(uint8_t *out);

// If the dirty flag is set, copy the payload into `out`, clear the flag and
// return true; otherwise return false (real-time polling mode).
bool report_buffer_get_if_dirty(uint8_t *out);

// Re-raise the dirty flag, e.g. after a failed USB enqueue so the report is
// retried on the next poll.
void report_buffer_set_dirty(void);

// Read a single payload byte under the lock (headset-bit / battery probes).
uint8_t report_buffer_get_byte(size_t index);

#ifdef __cplusplus
}
#endif
