#pragma once
// STUB di lwip/tcpip.h (solo per controllare la sintassi)
#include "lwip/netif.h"
typedef void (*tcpip_callback_fn)(void* ctx);
err_t tcpip_callback(tcpip_callback_fn function, void* ctx);
#define ERR_OK 0
