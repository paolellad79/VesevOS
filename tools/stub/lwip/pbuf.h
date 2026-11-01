// stub: solo per controllare la sintassi
#pragma once
#include <stdint.h>
struct pbuf { struct pbuf* next; void* payload; uint16_t tot_len; uint16_t len; };
// Copia len byte da offset in poi (firma come in lwIP 2.1); nel stub non fa niente.
inline uint16_t pbuf_copy_partial(const struct pbuf*, void*, uint16_t, uint16_t) { return 0; }
