#pragma once
// STUB di lwip/ip_addr.h (solo IPv4)
#include <stdint.h>
typedef struct { uint32_t addr; } ip4_addr_t;
#if CONFIG_LWIP_IPV6
typedef struct { uint32_t addr[4]; uint8_t zone; } ip6_addr_t;
typedef struct { union { ip6_addr_t ip6; ip4_addr_t ip4; } u_addr; uint8_t type; } ip_addr_t;
#define IPADDR_TYPE_V6 6U
#define IP_SET_TYPE_VAL(ipaddr, iptype) ((ipaddr).type = (iptype))
#define ip_2_ip6(ipaddr) (&((ipaddr)->u_addr.ip6))
#else
typedef struct { union { ip4_addr_t ip4; } u_addr; uint8_t type; } ip_addr_t;
#endif
#define IP_ADDR4(ipaddr, a, b, c, d) do { (ipaddr)->u_addr.ip4.addr = ((uint32_t)(d) << 24) | ((uint32_t)(c) << 16) | ((uint32_t)(b) << 8) | (uint32_t)(a); (ipaddr)->type = 0; } while (0)
#define IP_ANY_TYPE (&ip_addr_any_stub)
static const ip_addr_t ip_addr_any_stub = {{{0}}, 0};
