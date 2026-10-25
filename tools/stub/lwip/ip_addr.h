#pragma once
// STUB di lwip/ip_addr.h (solo IPv4)
#include <stdint.h>
typedef struct { uint32_t addr; } ip4_addr_t;
typedef struct { union { ip4_addr_t ip4; } u_addr; uint8_t type; } ip_addr_t;
#define IP_ADDR4(ipaddr, a, b, c, d) do { (ipaddr)->u_addr.ip4.addr = ((uint32_t)(d) << 24) | ((uint32_t)(c) << 16) | ((uint32_t)(b) << 8) | (uint32_t)(a); (ipaddr)->type = 0; } while (0)
#define IP_ANY_TYPE (&ip_addr_any_stub)
static const ip_addr_t ip_addr_any_stub = {{{0}}, 0};
