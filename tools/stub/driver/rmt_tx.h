#pragma once
#include "rmt_encoder.h"
#include "gpio.h"
typedef struct rmt_channel_t* rmt_channel_handle_t;
typedef enum { RMT_CLK_SRC_DEFAULT } rmt_clock_source_t;
typedef struct { gpio_num_t gpio_num; rmt_clock_source_t clk_src; uint32_t resolution_hz; size_t mem_block_symbols; size_t trans_queue_depth; } rmt_tx_channel_config_t;
typedef struct { int loop_count; struct { unsigned eot_level; } flags; } rmt_transmit_config_t;
esp_err_t rmt_new_tx_channel(const rmt_tx_channel_config_t*, rmt_channel_handle_t*);
esp_err_t rmt_del_channel(rmt_channel_handle_t);
esp_err_t rmt_enable(rmt_channel_handle_t);
esp_err_t rmt_disable(rmt_channel_handle_t);
esp_err_t rmt_transmit(rmt_channel_handle_t, rmt_encoder_handle_t, const void*, size_t, const rmt_transmit_config_t*);
esp_err_t rmt_tx_wait_all_done(rmt_channel_handle_t, int);
