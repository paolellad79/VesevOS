#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef struct { uint32_t freq_mhz; } rtc_cpu_freq_config_t;
void rtc_clk_cpu_freq_get_config(rtc_cpu_freq_config_t*);
bool rtc_clk_cpu_freq_mhz_to_config(uint32_t mhz, rtc_cpu_freq_config_t* out);
void rtc_clk_cpu_freq_set_config_fast(const rtc_cpu_freq_config_t* conf);
