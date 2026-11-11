#ifndef __RESOURCE_H
#define __RESOURCE_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

extern SemaphoreHandle_t xEffectsMutex;
void resource_init(void);

#endif
