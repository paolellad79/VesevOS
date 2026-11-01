// Finto lwip/prot/dhcp.h per le prove sul computer: solo gli stati del client DHCP (stessi valori di lwIP)
#pragma once
enum { DHCP_STATE_OFF = 0, DHCP_STATE_REQUESTING = 1, DHCP_STATE_INIT = 2, DHCP_STATE_REBOOTING = 3, DHCP_STATE_REBINDING = 4, DHCP_STATE_RENEWING = 5, DHCP_STATE_SELECTING = 6 };
