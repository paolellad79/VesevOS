#pragma once
#include "esp_err.h"
typedef struct temperature_sensor_obj_t* temperature_sensor_handle_t;
typedef struct { int range_min; int range_max; int clk_src; struct { int allow_pd; } flags; } temperature_sensor_config_t;
#define TEMPERATURE_SENSOR_CLK_SRC_DEFAULT 0
#define TEMPERATURE_SENSOR_CONFIG_DEFAULT(min, max) { .range_min = min, .range_max = max, .clk_src = 0, .flags = { .allow_pd = 0 } }
esp_err_t temperature_sensor_install(const temperature_sensor_config_t*, temperature_sensor_handle_t*);
esp_err_t temperature_sensor_uninstall(temperature_sensor_handle_t);
esp_err_t temperature_sensor_enable(temperature_sensor_handle_t);
esp_err_t temperature_sensor_get_celsius(temperature_sensor_handle_t, float*);
