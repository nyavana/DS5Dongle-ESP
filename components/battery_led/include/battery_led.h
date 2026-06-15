//
// Low-battery LED indicator for the ESP32-S31 board LED.
//
// Reads PowerPercent / PowerState from report_buffer byte 52
// (DualSense BT 0x31 report, see USBGetStateData in utils.h).
//

#pragma once

void battery_led_init(void);

// Periodic driver. The component starts a FreeRTOS task from battery_led_init(),
// but this remains public to preserve the Pico module contract.
void battery_led_tick(void);

// Called from the BT input-report callback whenever a fresh 0x31 report has
// been copied into report_buffer.
void battery_led_note_report(void);

// Called from the BT disconnect handler. Cancels any blink, forces the LED off,
// and ignores the cached battery byte until a fresh report arrives.
void battery_led_on_disconnect(void);

// Called after config updates so disable_pico_led takes effect immediately.
void battery_led_apply_config(void);
