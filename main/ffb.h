#ifndef __FFB_H
#define __FFB_H

#include "usb_hid_ffb_desc.h"
#include <stdint.h>

void ffb_hid_set_report_cb(uint8_t report_id, hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t bufsize);
uint16_t ffb_hid_get_report_cb(uint8_t report_id, hid_report_type_t report_type,
                               uint8_t *buffer, uint16_t reqlen);
void ffb_effect_impl(void);
#endif
