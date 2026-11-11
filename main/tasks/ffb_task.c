#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>
#include <stdio.h>
#include "ffb.h"
#include "encoder.h"
#include "usb_hid.h"
#include "motor.h"

bool angle_report(void){
  uint8_t buffer[24] = {0};
  int count = enc_get_count(); //count 范围 -ENC_CNT_LIMIT ~ ENC_CNT_LIMIT
  // int count = enc_get_count_test(); //count 范围 -ENC_CNT_LIMIT ~ ENC_CNT_LIMIT
  int16_t X = (int16_t)((float)count / ENC_CNT_LIMIT * 32767.0);
  buffer[8] = (uint8_t)(X & 0x00FF);
  buffer[9] = (uint8_t)(X >> 8);
  return send_gamepad_report(buffer);
}

static void ffb_task(void *args) {
  for (;;) {
    ffb_effect_impl();
    angle_report();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void ffb_task_create(void) {
  xTaskCreatePinnedToCore(ffb_task, "ffb", 4096, NULL, 4, NULL, 1);
}
