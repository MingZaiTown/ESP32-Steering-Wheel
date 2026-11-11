#ifndef __MOTOR_H
#define __MOTOR_H
#include "stdint.h"

#define MOT_CLK_FREQ 10000000
#define MOT_PWM_FREQ 25000
// #define MOT_MAX_SPEED ((float)MOT_CLK_FREQ / MOT_PWM_FREQ)
#define MOT_MAX_SPEED 100
#define MOT_GPIO_PWMA GPIO_NUM_1
#define MOT_GPIO_PWMB GPIO_NUM_2
#define MOT_GPIO_ENA GPIO_NUM_41
#define MOT_GPIO_ENB GPIO_NUM_42

void motor_init(void);
void motor_set_speed(uint32_t speed);
void motor_set_direction(bool direction);

#endif
