#pragma once
#include <stdint.h>
#include <sys/time.h>
void sntp_set_sync_interval(uint32_t); typedef void (*sntp_sync_time_cb_t)(struct timeval*); void sntp_set_time_sync_notification_cb(sntp_sync_time_cb_t);
void configTzTime(const char*,const char*,const char* =nullptr,const char* =nullptr);
