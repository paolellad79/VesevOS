// Finto FreeRTOS per le prove sul computer: un solo thread, i blocchi non fanno niente
#pragma once
#include <stdint.h>
typedef void* SemaphoreHandle_t;
#define pdMS_TO_TICKS(ms) (ms)
#define portMAX_DELAY 0xffffffffu
inline void vTaskDelay(uint32_t) {}
inline void taskYIELD() {}
