#include "usb_hid.h"
#include "class/hid/hid_device.h"
#include "esp_check.h"
#include "ffb.h"
#include "freertos/FreeRTOS.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "usb_hid_ffb_desc.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/_intsup.h>

#define TUSB_DESC_TOTAL_LEN                                                    \
  (TUD_CONFIG_DESC_LEN + CFG_TUD_HID * TUD_HID_INOUT_DESC_LEN)

const uint8_t hid_1ffb_desc[] = {
    0x05, 0x01, /* USAGE_PAGE (Generic Desktop)*/
    0x09, 0x04, /* USAGE (Joystick)*/
    0xa1, 0x01, /* COLLECTION (Application)*/
    HIDDESC_GAMEPAD_16B,

    HIDDESC_FFB_STATEREP,

    HIDDESC_FFB_SETEFREP,

    0x09, 0x55, //    Usage Axes Enable TODO multi axis
    0xA1, 0x02, //    Collection Datalink
    0x05, 0x01, //    Usage Page Generic Desktop
    0x09, 0x30, //    Usage X
    // 0x09,0x31,    //    Usage Y
    0x15, 0x00, //    Logical Minimum 0
    0x25, 0x00, //    Logical Maximum 0
    0x75, 0x01, //    Report Size 1
    0x95, 0x01, //    Report Count 1
    0x91, 0x02, //    Output (Variable)
    0xC0,       // End Collection
    0x05, 0x0F, //    Usage Page Physical Interface
    0x09, 0x56, //    Usage Direction Enable
    0x95, 0x01, //    Report Count 1
    0x91, 0x02, //    Output (Variable)
    0x95, 0x06, //    Report Count 6
    0x91, 0x03, //    Output (Constant, Variable)

    0x09, 0x57,                   //    Usage Direction
    0xA1, 0x02,                   //    Collection Datalink
    0x0B, 0x01, 0x00, 0x0A, 0x00, //    Usage Ordinals: Instance 1
    //			      0x0B,0x02,0x00,0x0A,0x00,    //    Usage Ordinals:
    // Instance 2
    0x66, 0x14, 0x00, //    Unit 14h (20d)
    //			      0x55,0xFE,                   //    Unit Exponent
    // FEh (254d) 			      0x15,0x00,                   //
    // Logical Minimum 0 			      0x26,0xFF,0x00, // Logical
    // Maximum FFh (255d)
    0x15, 0x00,                   //    Logical Minimum 0
    0x27, 0xA0, 0x8C, 0x00, 0x00, //    Logical Maximum 8CA0h (36000d)
    0x35, 0x00,                   //    Physical Minimum 0
    0x47, 0xA0, 0x8C, 0x00, 0x00, //    Physical Maximum 8CA0h (36000d)
    0x66, 0x00, 0x00,             //    Unit 0
    0x75, 0x10,                   //    Report Size 16
    0x95, 0x01,                   //    Report Count 1
    0x91, 0x02,                   //    Output (Variable)
    0x55, 0x00,                   //    Unit Exponent 0
    0x66, 0x00, 0x00,             //    Unit 0
    0xC0,                         //    End Collection

    0x05, 0x0F,                   //     USAGE_PAGE (Physical Interface)
    0x09, 0x58,                   //     USAGE (Type Specific Block Offset)
    0xA1, 0x02,                   //     COLLECTION (Logical)
    0x0B, 0x01, 0x00, 0x0A, 0x00, // USAGE (Ordinals:Instance 1
    // 0x0B, 0x02, 0x00, 0x0A, 0x00, //USAGE (Ordinals:Instance 2)
    0x26, 0xFD, 0x7F, //   LOGICAL_MAXIMUM (32765) ; 32K RAM or ROM max.
    0x75, 0x10,       //     REPORT_SIZE (16)
    0x95, 0x01,       //     REPORT_COUNT (1)
    0x91, 0x02,       //     OUTPUT (Data,Var,Abs)
    0xC0,             //     END_COLLECTION
    0xC0,             //     END_COLLECTION

    HIDDESC_FFB_SETENVREP,

    0x09, 0x5F,                           //    Usage Set Condition Report
    0xA1, 0x02,                           //    Collection Datalink
    0x85, HID_ID_CONDREP + FFB_ID_OFFSET, //    Report ID 3
    0x09, 0x22,                           //    Usage Effect Block Index
    0x15, 0x01,                           //    Logical Minimum 1
    0x25, MAX_EFFECTS,                    //    Logical Maximum 28h (40d)
    0x35, 0x01,                           //    Physical Minimum 1
    0x45, MAX_EFFECTS,                    //    Physical Maximum 28h (40d)
    0x75, 0x08,                           //    Report Size 8
    0x95, 0x01,                           //    Report Count 1
    0x91, 0x02,                           //    Output (Variable)
    0x09, 0x23,                           //    Usage Parameter Block Offset
    0x15, 0x00,                           //    Logical Minimum 0
    0x25, 0x03,                           //    Logical Maximum 3
    0x35, 0x00,                           //    Physical Minimum 0
    0x45, 0x03,                           //    Physical Maximum 3
    0x75, 0x06,                           //    Report Size 6
    0x95, 0x01,                           //    Report Count 1
    0x91, 0x02,                           //    Output (Variable)
    0x09, 0x58,                           //    Usage Type Specific Block Off...
    0xA1, 0x02,                           //    Collection Datalink
    0x0B, 0x01, 0x00, 0x0A, 0x00,         //    Usage Ordinals: Instance 1
    //			      0x0B,0x02,0x00,0x0A,0x00,    //    Usage Ordinals:
    // Instance 2
    0x75, 0x02,       //    Report Size 2
    0x95, 0x01,       //    Report Count 1
    0x91, 0x02,       //    Output (Variable)
    0xC0,             //    End Collection
    0x16, 0x00, 0x80, //    Logical Minimum 7FFFh (-32767d)
    0x26, 0xff, 0x7f, //    Logical Maximum 7FFFh (32767d)
    0x36, 0x00, 0x80, //    Physical Minimum 7FFFh (-32767d)
    0x46, 0xff, 0x7f, //    Physical Maximum 7FFFh (32767d)

    0x09, 0x60,       //    Usage CP Offset
    0x75, 0x10,       //    Report Size 16
    0x95, 0x01,       //    Report Count 1
    0x91, 0x02,       //    Output (Variable)
    0x36, 0x00, 0x80, //    Physical Minimum  (-32768)
    0x46, 0xff, 0x7f, //    Physical Maximum  (32767)
    0x09, 0x61,       //    Usage Positive Coefficient
    0x09, 0x62,       //    Usage Negative Coefficient
    0x95, 0x02,       //    Report Count 2
    0x91, 0x02,       //    Output (Variable)
    0x16, 0x00, 0x00, //    Logical Minimum 0
    0x26, 0xff, 0x7f, //    Logical Maximum  (32767)
    0x36, 0x00, 0x00, //    Physical Minimum 0
    0x46, 0xff, 0x7f, //    Physical Maximum  (32767)
    0x09, 0x63,       //    Usage Positive Saturation
    0x09, 0x64,       //    Usage Negative Saturation
    0x75, 0x10,       //    Report Size 16
    0x95, 0x02,       //    Report Count 2
    0x91, 0x02,       //    Output (Variable)
    0x09, 0x65,       //    Usage Dead Band
    0x46, 0xff, 0x7f, //    Physical Maximum (32767)
    0x95, 0x01,       //    Report Count 1
    0x91, 0x02,       //    Output (Variable)
    0xC0,             //    End Collection

    HIDDESC_FFB_SETPERIODICREP,

    HIDDESC_FFB_SETCFREP,

    HIDDESC_FFB_SETRAMPREP,

    //			0x09,0x68,    //    Usage Custom Force Data Report
    //			0xA1,0x02,    //    Collection Datalink
    //			   0x85,HID_ID_CSTMREP+FFB_ID_OFFSET,         // Report
    // ID 7 			   0x09,0x22,         //    Usage Effect Block
    // Index 			   0x15,0x01, // Logical Minimum 1
    // 0x25,MAX_EFFECTS,         //    Logical Maximum 28h (40d)
    // 0x35,0x01,         //    Physical Minimum 1
    // 0x45,MAX_EFFECTS, // Physical Maximum 28h (40d)
    // 0x75,0x08,         //    Report Size 8 			   0x95,0x01, //
    // Report
    // Count 1 			   0x91,0x02,         //    Output (Variable)
    // 0x09,0x6C,         // Usage Custom Force Data Offset
    // 0x15,0x00,         //
    // Logical Minimum 0 			   0x26,0x10,0x27,    // Logical
    // Maximum 2710h (10000d) 			   0x35,0x00,         //
    // Physical Minimum 0 			   0x46,0x10,0x27,    //
    // Physical Maximum 2710h
    //(10000d) 			   0x75,0x10,         //    Report Size 10h
    //(16d) 			   0x95,0x01, // Report Count 1
    // 0x91,0x02,         //    Output (Variable) 0x09,0x69, // Usage Custom
    // Force Data 			   0x15,0x81,         //    Logical
    // Minimum 81h
    //(-127d) 			   0x25,0x7F,         //    Logical Maximum 7Fh
    //(127d) 			   0x35,0x00, // Physical Minimum 0
    // 0x46,0xFF,0x00,    //    Physical Maximum FFh (255d)
    // 0x75,0x08, // Report
    // Size 8 			   0x95,0x0C,         //    Report Count Ch
    // (12d) 			   0x92,0x02,0x01, // Output (Variable,
    // Buffered) 			0xC0     ,    //    End
    // Collection 			0x09,0x66,    //    Usage Download Force
    // Sample 			0xA1,0x02,    //    Collection Datalink
    //			   0x85,HID_ID_SMPLREP+FFB_ID_OFFSET,         // Report
    // ID 8 			   0x05,0x01,         //    Usage Page Generic
    // Desktop 			   0x09,0x30, // Usage X
    // 0x09,0x31,         //    Usage Y 			   0x15,0x81, //
    // Logical Minimum 81h (-127d) 			   0x25,0x7F,         //
    // Logical Maximum 7Fh (127d) 			   0x35,0x00,         //
    // Physical Minimum 0
    //			   0x46,0xFF,0x00,    //    Physical Maximum FFh (255d)
    //			   0x75,0x08,         //    Report Size 8
    //			   0x95,0x02,         //    Report Count 2
    //			   0x91,0x02,         //    Output (Variable)
    //			0xC0     ,   //    End Collection

    HIDDESC_FFB_EFOPREP, HIDDESC_FFB_BLOCKFREEREP,

    HIDDESC_FFB_DEVCTRLREP,
    //			0x09,0x6B,    //    Usage Set Custom Force Report
    //			0xA1,0x02,    //    Collection Datalink
    //			   0x85,HID_ID_SETCREP+FFB_ID_OFFSET,         // Report
    // ID Eh (14d) 			   0x09,0x22,         //    Usage Effect
    // Block Index 			   0x15,0x01,
    ////    Logical Minimum 1 			   0x25,MAX_EFFECTS,         //
    /// Logical Maximum
    // 28h (40d) 			   0x35,0x01,         //    Physical
    // Minimum 1 			   0x45,MAX_EFFECTS, // Physical Maximum
    // 28h (40d) 			   0x75,0x08,         //    Report Size
    // 8 			   0x95,0x01,         //    Report Count 1
    // 0x91,0x02,         //    Output
    //(Variable) 			   0x09,0x6D,         //    Usage Sample
    // Count 			   0x15,0x00, // Logical Minimum 0
    // 0x26,0xFF,0x00,    //    Logical Maximum FFh (255d)
    // 0x35,0x00,         //
    // Physical Minimum 0 			   0x46,0xFF,0x00,    //
    // Physical Maximum FFh (255d) 			   0x75,0x08,         //
    // Report Size 8
    //			   0x95,0x01,         //    Report Count 1
    //			   0x91,0x02,         //    Output (Variable)
    //			   0x09,0x51,         //    Usage Sample Period
    //			   0x66,0x03,0x10,    //    Unit 1003h (4099d)
    //			   0x55,0xFD,         //    Unit Exponent FDh (253d)
    //			   0x15,0x00,         //    Logical Minimum 0
    //			   0x26,0xFF,0x7F,    //    Logical Maximum 7FFFh
    //(32767d) 			   0x35,0x00,         //    Physical Minimum 0
    // 0x46,0xFF,0x7F,    // Physical Maximum 7FFFh (32767d)
    // 0x75,0x10,         //    Report Size 10h (16d)
    // 0x95,0x01,         //    Report Count 1 			   0x91,0x02, //
    // Output (Variable) 			   0x55,0x00,         //    Unit
    // Exponent 0 			   0x66,0x00,0x00,    // Unit 0
    // 0xC0     ,    //    End Collection
    HIDDESC_FFB_NEWEFREP, HIDDESC_FFB_BLOCKLOADREP,

    HIDDESC_FFB_POOLREP,

    0xC0 /*     END_COLLECTION	             */
};

/**
 * @brief String descriptor
 */
const char *hid_string_descriptor[5] = {
    // array of pointer to string descriptors
    (char[]){0x09, 0x04}, // 0: is supported language is English (0x0409)
    "ESP32",              // 1: Manufacturer
    "steering wheel",     // 2: Product
    "MINGZAI20261106",    // 3: Serials, should use chip ID
    "steering wheel",     // 4: HID
};

/**
 * @brief Configuration descriptor
 */
static const uint8_t hid_configuration_descriptor[] = {
    // Configuration number, interface count, string index, total length,
    // attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUSB_DESC_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface number, string index, boot protocol, report descriptor len, EP
    // In address, size & polling interval
    TUD_HID_INOUT_DESCRIPTOR(0, 4, false, sizeof(hid_1ffb_desc), 0x01, 0x81, 64,
                             1),
};

// 不用管
//  Invoked when received GET HID REPORT DESCRIPTOR request
//  Application return pointer to descriptor, whose contents must exist long
//  enough for transfer to complete
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
  // We use only one interface and one HID report descriptor, so we can ignore
  // parameter 'instance'
  return hid_1ffb_desc;
}

// 主机读设备信息
//  Invoked when received GET_REPORT control request
//  Application must fill buffer report's content and return its length.
//  Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t reqlen) {
  return ffb_hid_get_report_cb(report_id, report_type, buffer, reqlen);
}

// 主机写给设备
//  Invoked when received SET_REPORT control request or
//  received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type, uint8_t const *buffer,
                           uint16_t bufsize) {
  ffb_hid_set_report_cb(report_id, report_type, buffer, bufsize);
}

void usb_hid_init(void) {
  tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();

  tusb_cfg.descriptor.device = NULL;
  tusb_cfg.descriptor.full_speed_config = hid_configuration_descriptor;
  tusb_cfg.descriptor.string = hid_string_descriptor;
  tusb_cfg.descriptor.string_count =
      sizeof(hid_string_descriptor) / sizeof(hid_string_descriptor[0]);
  ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
}

// 设备主动上报
bool send_gamepad_report(const uint8_t *buf) { 
  return tud_hid_report(0x01, buf, 24);
}
bool send_status(uint8_t status) { return false; }
