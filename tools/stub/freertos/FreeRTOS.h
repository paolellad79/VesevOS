#pragma once
#include <stdint.h>
typedef void* TaskHandle_t; typedef void* SemaphoreHandle_t; typedef void* QueueHandle_t; typedef void* TimerHandle_t; typedef uint32_t TickType_t; typedef int BaseType_t; typedef unsigned UBaseType_t;
#define pdMS_TO_TICKS(x) (x)
#define portMAX_DELAY 0xffffffff
#define portTICK_PERIOD_MS 1
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define tskNO_AFFINITY 0x7FFFFFFF
inline void vTaskDelay(TickType_t){} inline BaseType_t xTaskCreatePinnedToCore(void(*)(void*),const char*,uint32_t,void*,UBaseType_t,TaskHandle_t*,BaseType_t){return 1;}
inline BaseType_t xTaskCreate(void(*)(void*),const char*,uint32_t,void*,UBaseType_t,TaskHandle_t*){return 1;}
inline SemaphoreHandle_t xSemaphoreCreateMutex(){return 0;} inline int xSemaphoreTake(SemaphoreHandle_t,TickType_t){return 1;} inline int xSemaphoreGive(SemaphoreHandle_t){return 1;}
inline QueueHandle_t xQueueCreate(UBaseType_t,UBaseType_t){return 0;} inline BaseType_t xQueueSend(QueueHandle_t,const void*,TickType_t){return 1;} inline BaseType_t xQueueReceive(QueueHandle_t,void*,TickType_t){return 0;} inline BaseType_t xQueueSendFromISR(QueueHandle_t,const void*,BaseType_t*){return 1;}
inline TickType_t xTaskGetTickCount(){return 0;} inline TaskHandle_t xTaskGetCurrentTaskHandle(){return 0;}
typedef enum{eRunning=0,eReady,eBlocked,eSuspended,eDeleted} eTaskState; typedef struct{TaskHandle_t xHandle;const char*pcTaskName;UBaseType_t xTaskNumber;eTaskState eCurrentState;UBaseType_t uxCurrentPriority;UBaseType_t uxBasePriority;uint32_t ulRunTimeCounter;uint32_t usStackHighWaterMark;BaseType_t xCoreID;} TaskStatus_t;
inline UBaseType_t uxTaskGetNumberOfTasks(){return 0;} inline UBaseType_t uxTaskGetSystemState(TaskStatus_t*,UBaseType_t,uint32_t*){return 0;} inline void vTaskList(char*){}
