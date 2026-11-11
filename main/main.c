#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tasks.h"
#include "encoder.h"
#include "motor.h"
#include "usb_hid.h"
#include "resource.h"

void app_main(void) {

  enc_init();
  motor_init();
  resource_init();

  usb_hid_init();
  ffb_task_create();
  vTaskDelete(NULL);

}
