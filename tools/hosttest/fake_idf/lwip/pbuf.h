// Finto lwip/pbuf.h per le prove sul computer: un pbuf e un blocco di memoria
#pragma once
#include <stdint.h>
#include <string.h>
struct pbuf { const uint8_t* data; uint16_t len; };
inline uint16_t pbuf_copy_partial(const struct pbuf* p, void* dst, uint16_t n, uint16_t off) {
  if ((uint32_t)off + n > p->len) return 0;
  memcpy(dst, p->data + off, n); return n;
}
