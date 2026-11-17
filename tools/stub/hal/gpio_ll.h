#pragma once
// STUB di hal/gpio_ll.h (solo le due funzioni che servono al LED del panico)
#include <stdint.h>
typedef struct { uint32_t pad; } gpio_dev_t;
extern gpio_dev_t GPIO;
static inline void gpio_ll_output_enable(gpio_dev_t*, uint32_t) {}
static inline void gpio_ll_set_level(gpio_dev_t*, uint32_t, uint32_t) {}
