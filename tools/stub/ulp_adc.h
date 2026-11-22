#pragma once
// Copia ridotta di ulp_adc.h (ESP-IDF 5.5.5, componente ulp)
#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"
typedef struct { adc_unit_t adc_n; adc_channel_t channel; adc_atten_t atten; adc_bitwidth_t width; adc_ulp_mode_t ulp_mode; } ulp_adc_cfg_t;
esp_err_t ulp_adc_init(const ulp_adc_cfg_t* cfg);
