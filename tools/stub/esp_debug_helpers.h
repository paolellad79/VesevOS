#pragma once
// STUB di esp_debug_helpers.h (percorso dei frame per il gancio del panico)
#include <stdint.h>
typedef struct { uint32_t pc; uint32_t sp; uint32_t next_pc; const void* exc_frame; } esp_backtrace_frame_t;
bool esp_backtrace_get_next_frame(esp_backtrace_frame_t* frame);
