#pragma once
#include "esp_err.h"
#include "driver/gpio.h"
typedef struct adc_oneshot_unit_ctx_t* adc_oneshot_unit_handle_t;
typedef enum { ADC_UNIT_1, ADC_UNIT_2 } adc_unit_t;
typedef int adc_channel_t;
typedef enum { ADC_ATTEN_DB_0, ADC_ATTEN_DB_12 = 3 } adc_atten_t;
typedef enum { ADC_BITWIDTH_DEFAULT, ADC_BITWIDTH_12 = 12 } adc_bitwidth_t;
typedef enum { ADC_ULP_MODE_DISABLE } adc_ulp_mode_t;
typedef struct { adc_unit_t unit_id; adc_ulp_mode_t ulp_mode; } adc_oneshot_unit_init_cfg_t;
typedef struct { adc_atten_t atten; adc_bitwidth_t bitwidth; } adc_oneshot_chan_cfg_t;
esp_err_t adc_oneshot_new_unit(const adc_oneshot_unit_init_cfg_t*, adc_oneshot_unit_handle_t*);
esp_err_t adc_oneshot_config_channel(adc_oneshot_unit_handle_t, adc_channel_t, const adc_oneshot_chan_cfg_t*);
esp_err_t adc_oneshot_read(adc_oneshot_unit_handle_t, adc_channel_t, int*);
esp_err_t adc_oneshot_io_to_channel(int, adc_unit_t*, adc_channel_t*);
