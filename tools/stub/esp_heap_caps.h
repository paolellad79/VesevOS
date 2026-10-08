// stub per la verifica di sintassi
#pragma once
#include <stddef.h>
#include <stdint.h>
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_SPIRAM 2
#define MALLOC_CAP_8BIT 4
typedef struct { size_t total_free_bytes, total_allocated_bytes, largest_free_block, minimum_free_bytes; size_t allocated_blocks, free_blocks, total_blocks; } multi_heap_info_t;
inline void heap_caps_get_info(multi_heap_info_t* i, uint32_t) { *i = multi_heap_info_t(); }
void* heap_caps_calloc(size_t n, size_t size, uint32_t caps);
void heap_caps_free(void* p);
size_t heap_caps_get_free_size(uint32_t caps);
size_t heap_caps_get_total_size(uint32_t caps);
size_t heap_caps_get_largest_free_block(uint32_t caps);
size_t heap_caps_get_minimum_free_size(uint32_t caps);
