#pragma once
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
typedef struct rmt_encoder_t* rmt_encoder_handle_t;
typedef struct { uint16_t duration0:15; uint16_t level0:1; uint16_t duration1:15; uint16_t level1:1; } rmt_symbol_word_t;
typedef struct { rmt_symbol_word_t bit0; rmt_symbol_word_t bit1; struct { unsigned msb_first; } flags; } rmt_bytes_encoder_config_t;
esp_err_t rmt_new_bytes_encoder(const rmt_bytes_encoder_config_t*, rmt_encoder_handle_t*);
esp_err_t rmt_del_encoder(rmt_encoder_handle_t);
