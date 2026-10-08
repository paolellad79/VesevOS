// stub: solo per controllare la sintassi (campi di lwIP 2.x usati da vos_traffic)
#pragma once
#include <stdint.h>
#include "lwip/pbuf.h"
typedef int8_t err_t;
struct netif;
typedef err_t (*netif_input_fn)(struct pbuf* p, struct netif* inp);
typedef err_t (*netif_linkoutput_fn)(struct netif* netif, struct pbuf* p);
struct netif { netif_input_fn input; netif_linkoutput_fn linkoutput; };
