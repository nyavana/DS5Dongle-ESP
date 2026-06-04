//
// BT <-> USB HID report bridge.
//
// Replaces the Pico super-loop's interrupt_loop() + on_bt_data() + the USB
// output-report path. on_bt_data is registered with the BT host; a FreeRTOS
// task forwards the buffered input report to USB per the polling-rate mode; and
// the TinyUSB HID SET/GET-report callbacks (defined in bridge.cpp) route the
// command / output / feature traffic.
//
#ifndef DS5_BRIDGE_BRIDGE_H
#define DS5_BRIDGE_BRIDGE_H

#include "bt.h" // CHANNEL_TYPE

// Register the BT data callback, init output state, and start the USB
// in-report task. Call after config_load() and bt_init().
void bridge_init();

// BT input-report callback (INTERRUPT/0x31 -> USB 0x01). Exposed so app_main /
// bt_register_data_callback can wire it.
void on_bt_data(CHANNEL_TYPE channel, uint8_t *data, uint16_t len);

// Set by the audio path (group 6) when the USB speaker interface goes active,
// so the standalone USB->BT 0x31 output relay is suppressed (output state then
// rides inside the audio 0x36 report).
void bridge_set_speaker_active(bool active);

#endif //DS5_BRIDGE_BRIDGE_H
