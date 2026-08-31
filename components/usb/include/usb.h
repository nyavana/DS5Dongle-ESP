//
// Created by awalol on 2026/3/4.  (ESP-IDF port)
//

#ifndef DS5_BRIDGE_USB_H
#define DS5_BRIDGE_USB_H

#include <cstdint>

extern uint8_t mute[2]; // 0: SPEAKER(0x02) 1: MIC(0x05)
extern float volume[2]; // 0: SPEAKER(0x02) 1: MIC(0x05)

// Install the esp_tinyusb device stack with the ported DualSense descriptors
// and start the USB device task.
void usb_init();

// Request USB re-enumeration. TinyUSB calls and reconnect timing remain owned
// by this component so command handling has no direct USB-stack dependency.
void usb_reconnect();

#endif //DS5_BRIDGE_USB_H
