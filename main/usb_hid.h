#ifndef __USB_HID_H
#define __USB_HID_H
#include <stdint.h>
#include <stdbool.h>

void usb_hid_init(void);
bool send_gamepad_report(const uint8_t *buf);
bool send_status(uint8_t status);

#endif
