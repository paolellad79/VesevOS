#pragma once
// STUB di lwip/ip6_addr.h (solo per controllare la sintassi)
#include "lwip/ip_addr.h"
enum { IP6_UNKNOWN = 0, IP6_UNICAST = 1 };
#define ip6_addr_assign_zone(ip6addr, type, netif) ((ip6addr)->zone = netif_get_index(netif))
