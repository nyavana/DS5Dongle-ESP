#include "report_buffer.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// DualSense default 0x31 -> 0x01 payload, verbatim from the Pico build's
// interrupt_in_data initializer. Sent until the first BT report arrives so the
// host sees a valid (neutral) controller state during enumeration.
static uint8_t s_report[REPORT_IN_LEN] = {
    0x7f, 0x7d, 0x7f, 0x7e, 0x00, 0x00, 0xa7,
    0x08, 0x00, 0x00, 0x00, 0x52, 0x43, 0x30, 0x41,
    0x01, 0x00, 0x0e, 0x00, 0xef, 0xff, 0x03, 0x03,
    0x7b, 0x1b, 0x18, 0xf0, 0xcc, 0x9c, 0x60, 0x00,
    0xfc, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00,
    0x00, 0x00, 0x09, 0x09, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xa7, 0xad, 0x60, 0x00, 0x29, 0x18, 0x00,
    0x53, 0x9f, 0x28, 0x35, 0xa5, 0xa8, 0x0c, 0x8b};

static volatile bool s_dirty = false;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

void report_buffer_init(void) {
    // Static initializers already hold the DualSense defaults; clear the dirty
    // flag so a soft restart starts from a clean, non-pending state.
    taskENTER_CRITICAL(&s_lock);
    s_dirty = false;
    taskEXIT_CRITICAL(&s_lock);
}

void report_buffer_store(const uint8_t *payload, bool mark_dirty) {
    taskENTER_CRITICAL(&s_lock);
    memcpy(s_report, payload, REPORT_IN_LEN);
    if (mark_dirty) {
        s_dirty = true;
    }
    taskEXIT_CRITICAL(&s_lock);
}

void report_buffer_get(uint8_t *out) {
    taskENTER_CRITICAL(&s_lock);
    memcpy(out, s_report, REPORT_IN_LEN);
    taskEXIT_CRITICAL(&s_lock);
}

bool report_buffer_get_if_dirty(uint8_t *out) {
    bool got = false;
    taskENTER_CRITICAL(&s_lock);
    if (s_dirty) {
        memcpy(out, s_report, REPORT_IN_LEN);
        s_dirty = false;
        got = true;
    }
    taskEXIT_CRITICAL(&s_lock);
    return got;
}

void report_buffer_set_dirty(void) {
    taskENTER_CRITICAL(&s_lock);
    s_dirty = true;
    taskEXIT_CRITICAL(&s_lock);
}

uint8_t report_buffer_get_byte(size_t index) {
    uint8_t v = 0;
    taskENTER_CRITICAL(&s_lock);
    if (index < REPORT_IN_LEN) {
        v = s_report[index];
    }
    taskEXIT_CRITICAL(&s_lock);
    return v;
}
