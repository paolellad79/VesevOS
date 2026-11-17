#pragma once
// STUB di driver/pulse_cnt.h: solo cio che usa drivers/vos_drv_contatore.cpp
#include "esp_err.h"
#include <stdint.h>
typedef struct pcnt_unit_t* pcnt_unit_handle_t;
typedef struct pcnt_channel_t* pcnt_channel_handle_t;
typedef enum { PCNT_CHANNEL_EDGE_ACTION_HOLD, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE } pcnt_channel_edge_action_t;
typedef struct { int low_limit; int high_limit; int intr_priority; struct { uint32_t accum_count: 1; } flags; } pcnt_unit_config_t;
typedef struct { int edge_gpio_num; int level_gpio_num; struct { uint32_t io_loop_back: 1; } flags; } pcnt_chan_config_t;
typedef struct { uint32_t max_glitch_ns; } pcnt_glitch_filter_config_t;
esp_err_t pcnt_new_unit(const pcnt_unit_config_t*, pcnt_unit_handle_t*);
esp_err_t pcnt_del_unit(pcnt_unit_handle_t);
esp_err_t pcnt_unit_set_glitch_filter(pcnt_unit_handle_t, const pcnt_glitch_filter_config_t*);
esp_err_t pcnt_unit_enable(pcnt_unit_handle_t);
esp_err_t pcnt_unit_disable(pcnt_unit_handle_t);
esp_err_t pcnt_unit_start(pcnt_unit_handle_t);
esp_err_t pcnt_unit_stop(pcnt_unit_handle_t);
esp_err_t pcnt_unit_clear_count(pcnt_unit_handle_t);
esp_err_t pcnt_unit_get_count(pcnt_unit_handle_t, int*);
esp_err_t pcnt_new_channel(pcnt_unit_handle_t, const pcnt_chan_config_t*, pcnt_channel_handle_t*);
esp_err_t pcnt_del_channel(pcnt_channel_handle_t);
esp_err_t pcnt_channel_set_edge_action(pcnt_channel_handle_t, pcnt_channel_edge_action_t, pcnt_channel_edge_action_t);
