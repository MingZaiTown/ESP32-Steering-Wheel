#include "motor.h"
#include "bdc_motor.h"
#include "driver/gpio.h"
#include <stdint.h>
#include <stdio.h>

bdc_motor_handle_t motor = NULL;

void motor_init(void) {
  bdc_motor_config_t motor_config = {
      .pwm_freq_hz = MOT_PWM_FREQ,
      .pwma_gpio_num = MOT_GPIO_PWMA,
      .pwmb_gpio_num = MOT_GPIO_PWMB,
  };
  bdc_motor_mcpwm_config_t mcpwm_config = {
      .group_id = 0,
      .resolution_hz = MOT_CLK_FREQ,
  };
  ESP_ERROR_CHECK(
      bdc_motor_new_mcpwm_device(&motor_config, &mcpwm_config, &motor));
  ESP_ERROR_CHECK(bdc_motor_enable(motor));
  ESP_ERROR_CHECK(bdc_motor_forward(motor));

  // en引脚
  gpio_config_t io_conf = {};
  // disable interrupt
  io_conf.intr_type = GPIO_INTR_DISABLE;
  // set as output mode
  io_conf.mode = GPIO_MODE_OUTPUT;
  // bit mask of the pins that you want to set,e.g.GPIO18/19
  io_conf.pin_bit_mask = BIT64(MOT_GPIO_ENA) | BIT64(MOT_GPIO_ENB);
  // disable pull-down mode
  io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  // disable pull-up mode
  io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  // configure GPIO with the given settings
  gpio_config(&io_conf);
  gpio_set_level(MOT_GPIO_ENA, 1);
  gpio_set_level(MOT_GPIO_ENB, 1);
}

void motor_set_speed(uint32_t speed) { 
      printf("speed:%d\n", (int16_t)speed);
    bdc_motor_set_speed(motor, (speed >= MOT_MAX_SPEED ? MOT_MAX_SPEED - 1 : speed)); 
}

void motor_set_direction(bool direction) {
  if (direction) {
    bdc_motor_forward(motor);
  } else {
    bdc_motor_reverse(motor);
  }
}
