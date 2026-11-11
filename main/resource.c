#include "resource.h"

SemaphoreHandle_t xEffectsMutex;
void resource_init(void) { 
  xEffectsMutex = xSemaphoreCreateMutex(); 
}
