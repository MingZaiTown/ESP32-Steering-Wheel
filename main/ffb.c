#include "ffb.h"
#include "device/usbd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "motor.h"
#include "portmacro.h"
#include "resource.h"
#include "usb_hid_ffb_desc.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef enum {
  EFFECT_NONE = 0,
  EFFECT_CONSTANT = 1,
  EFFECT_RAMP = 2,
  EFFECT_SQUARE = 3,
  EFFECT_SINE = 4,
  EFFECT_TRIANGLE = 5,
  EFFECT_SAW_UP = 6,
  EFFECT_SAW_DOWN = 7,
  EFFECT_SPRING = 8,
  EFFECT_DAMPER = 9,
  EFFECT_INERTIA = 10,
  EFFECT_FRICTION = 11,
} effect_type_t;

typedef struct {
  // ---- 来自 0x11 / 0x01 的公共信息 ----
  bool in_use;
  effect_type_t type; // effect_type_t
  bool active;        // 0x0A start/stop
  uint8_t trigger_button;
  uint16_t duration;    // ms
  uint16_t start_delay; // ms
  uint16_t gain;        // 0..255
  uint16_t direction;   // 0..36000 (0.01°)，可用来定正负
  uint8_t loop_count;
  uint32_t start_tick; // 本地记录开始时刻（算 duration/envelope/ramp 用）

  // ---- 包络 (0x02) ----
  uint16_t attack_level, fade_level;
  uint32_t attack_time, fade_time;

  // ---- 类型专有参数 ----
  union {
    struct {
      int16_t magnitude;
    } constant; // 0x05
    struct {
      int16_t start, end;
    } ramp; // 0x06
    struct {
      int16_t magnitude, offset; // 0x04
      uint16_t phase;
      uint32_t period;
    } periodic;
    struct {
      int16_t cp_offset; // 0x03
      int16_t pos_coef, neg_coef;
      int16_t pos_sat, neg_sat;
      int16_t deadband;
    } condition;
  } p;
} effect_t;

static effect_t effects[MAX_EFFECTS + 1]; // 用 1..8，0 不用
static uint8_t lst_blk_idx;

// 主机读设备信息
uint16_t ffb_hid_get_report_cb(uint8_t report_id, hid_report_type_t report_type,
                               uint8_t *buffer, uint16_t reqlen) {
  switch (report_id) {
  case 0x13:                 // PID Pool
    buffer[0] = 0xFF;        // RAM Pool Size low
    buffer[1] = 0xFF;        // RAM Pool Size high
    buffer[2] = MAX_EFFECTS; // Simultaneous Effects Max = 8
    buffer[3] = 0x01;        // bit0 Device Managed Pool = 1, bit1 Shared = 0
    printf("[GET] id = 0x13\n");
    return 4;
  case 0x12: // Block Load
    if (lst_blk_idx != 0) {
      buffer[0] = lst_blk_idx; // Effect Block Index
      buffer[1] = 0x01;        // Status = 1 (Success)
      buffer[2] = 0xFF;        // Pool Available low
      buffer[3] = 0xFF;        // Pool Available high
      printf("[GET] id = 0x12: %d\n", lst_blk_idx);
    } else {
      buffer[0] = 0;    // Effect Block Index
      buffer[1] = 2;    // Status = 2 (full)
      buffer[2] = 0xFF; // Pool Available low
      buffer[3] = 0xFF; // Pool Available high
      printf("[GET] id = 0x12: %d\n", lst_blk_idx);
    }
    return 4;
  }
  return 0;
}

#define FEED_TIMEOUT 1000
uint32_t Lst_feed_time;
void ffb_feed_dog(void) { Lst_feed_time = xTaskGetTickCount(); }

// 主机写给设备
void ffb_hid_set_report_cb(uint8_t report_id, hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t bufsize) {

  static bool en_0x01 = true;
  static bool lst_active_0x0A = false;
  char log_text[64] = {0};
  if (xSemaphoreTake(xEffectsMutex, pdMS_TO_TICKS(2)) == pdTRUE) {
    if (report_id != 0) { // 就是0x11，申请槽位
      if (report_id == 0x11) {
        lst_blk_idx = 0;
        for (uint8_t i = 1; i < MAX_EFFECTS + 1; i++) {
          if (!effects[i].in_use) { // 空闲槽位
            lst_blk_idx = i;
            effects[i].in_use = true;
            break;
          }
        }
        snprintf(log_text, sizeof(log_text), "[SET] 0x11: %d\n", lst_blk_idx);
      }
    } else {
      if (buffer[0] == 0x0C || buffer[0] == 0x0D) { // 全局不需要索引判断
        if (buffer[0] == 0x0C) {                    // Device Control
          uint8_t bits = buffer[1] & 0x3F;
          if (bits & 0x01) { // Enable Actuators
          }
          if (bits & 0x02) { // Disable Actuators
          }
          if (bits & 0x04) { // Stop All Effects
            for (uint8_t i = 1; i < MAX_EFFECTS + 1; i++) {
              effects[i].active = false;
            }
          }
          if ((bits & 0x08) != 0) { // Device Reset
            lst_blk_idx = 0;
            memset(effects, 0, (MAX_EFFECTS + 1) * sizeof(effect_t));
          }
          if (bits & 0x10) { // Device Pause
            for (uint8_t i = 1; i < MAX_EFFECTS + 1; i++) {
              effects[i].active = false;
            }
          }
          if (bits & 0x20) { // Device Continue
            for (uint8_t i = 1; i < MAX_EFFECTS + 1; i++) {
              if (effects[i].in_use) {
                effects[i].active = true;
              }
            }
          }
          snprintf(log_text, sizeof(log_text),"[SET] 0x0C: %d\n", bits);

        } else { // Device Gain
          snprintf(log_text, sizeof(log_text),"[SET] 0x0D\n");
          // pass
        }
      } else { // 与block相关的操作
        uint8_t idx = buffer[1];
        if (idx <= MAX_EFFECTS && idx > 0) { // 索引超界判断
          switch (buffer[0]) {
          case 0x01: // Set Effect
            effects[idx].type = buffer[2];
            effects[idx].duration = (uint16_t)(buffer[3] | buffer[4] << 8);
            effects[idx].start_delay = (uint16_t)(buffer[9] | buffer[10] << 8);
            effects[idx].gain = buffer[11];
            effects[idx].trigger_button = buffer[12];
            effects[idx].direction = (uint16_t)(buffer[14] | buffer[15] << 8);
            if (en_0x01) {
              snprintf(log_text, sizeof(log_text), "[SET] 0x01\n");
              en_0x01 = false;
            }
            break;
          case 0x05: // Set Constant Force
            effects[idx].p.constant.magnitude =
                (int16_t)(buffer[2] | buffer[3] << 8);
            if (effects[idx].p.constant.magnitude > 500 ||
                effects[idx].p.constant.magnitude < -500) {
              snprintf(log_text, sizeof(log_text), "[SET] 0x05: %d\n",
                       effects[idx].p.constant.magnitude);
            }
            break;
          case 0x0A: // Effect Operation（播放/停止）
            lst_active_0x0A = effects[idx].active;
            effects[idx].active =
                (buffer[2] == 1) || (buffer[2] == 2); // start or loop
            effects[idx].loop_count = buffer[3];
            if (lst_active_0x0A != effects[idx].active) {
              snprintf(log_text, sizeof(log_text), "[SET] 0x0A: %d %d\n",
                       effects[idx].active, effects[idx].loop_count);
            }
            break;
          case 0x0B: // Block Free（1 字节）
            memset(&effects[idx], 0, sizeof(effect_t));
            snprintf(log_text, sizeof(log_text), "[SET] 0x0B: Block Free:%d\n", idx);
            break;
          }
        }
      }
    }
    ffb_feed_dog();
    xSemaphoreGive(xEffectsMutex);
  };
  if (log_text[0] != 0) {
    printf("%s", log_text);
  }
}

#define FFB_MAX_MAGNITUDE 1000
#define FFB_SCALE (FFB_MAX_MAGNITUDE / MOT_MAX_SPEED)

void ffb_effect_impl(void) {
  int32_t magnitude_raw_sum = 0;
  uint16_t magnitude_abs = 0;
  uint32_t speed = 0;
  if (xSemaphoreTake(xEffectsMutex, 0) == pdTRUE) {
    for (uint8_t i = 1; i < MAX_EFFECTS + 1; i++) {
      if (effects[i].in_use && effects[i].active) {
        switch (effects[i].type) {
        case EFFECT_CONSTANT:
          magnitude_raw_sum += effects[i].p.constant.magnitude;
          break;
        default:
          break;
        }
      }
    }
    motor_set_direction(magnitude_raw_sum > 0);
    if (magnitude_raw_sum > 0) { // 饱和
      if (magnitude_raw_sum > FFB_MAX_MAGNITUDE) {
        magnitude_abs = FFB_MAX_MAGNITUDE;
      } else {
        magnitude_abs = magnitude_raw_sum;
      }
    } else {
      if (magnitude_raw_sum < -FFB_MAX_MAGNITUDE) {
        magnitude_abs = FFB_MAX_MAGNITUDE;
      } else {
        magnitude_abs = -magnitude_raw_sum;
      }
    }

    speed = (uint32_t)(magnitude_abs / FFB_SCALE);
    motor_set_speed(speed);
    xSemaphoreGive(xEffectsMutex);
  }
  // if ((xTaskGetTickCount() - Lst_feed_time) > pdMS_TO_TICKS(FEED_TIMEOUT) ||
  //     !tud_mounted()) { // watch dog
  //   motor_set_speed(0);
  //   printf("watch dog time out\n");
  // }
}
