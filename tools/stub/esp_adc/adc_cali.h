#pragma once
#include "adc_oneshot.h"
typedef struct adc_cali_scheme_t* adc_cali_handle_t;
esp_err_t adc_cali_raw_to_voltage(adc_cali_handle_t, int, int*);
