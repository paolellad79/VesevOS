#pragma once
// STUB di lwip/dns.h (solo per controllare la sintassi)
#include "lwip/ip_addr.h"
#define DNS_FALLBACK_SERVER_INDEX 2
void dns_setserver(uint8_t numdns, const ip_addr_t* dnsserver);
