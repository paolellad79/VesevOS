#pragma once
#include "FreeRTOS.h"
void vTaskDelete(TaskHandle_t); UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t);   // iguali a quelli di Arduino.h (stub): niente definizione, i file si compilano e non si collegano
TaskHandle_t xTaskGetHandle(const char*);
