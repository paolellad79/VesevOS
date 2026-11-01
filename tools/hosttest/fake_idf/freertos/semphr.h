#pragma once
#include "FreeRTOS.h"
inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutex() { return (void*)1; }
inline int xSemaphoreTakeRecursive(SemaphoreHandle_t, uint32_t) { return 1; }
inline int xSemaphoreGiveRecursive(SemaphoreHandle_t) { return 1; }
