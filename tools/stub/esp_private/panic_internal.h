#pragma once
// STUB di esp_private/panic_internal.h (solo il tipo che serve al gancio del panico)
typedef struct { int core; int exception; const char* reason; const char* description; const void* addr; const void* frame; } panic_info_t;
