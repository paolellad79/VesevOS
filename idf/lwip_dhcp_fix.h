/* VesevOS - correzione per ESP-IDF 5.5.5 (opzione CONFIG_LWIP_HOOK_DHCP_EXTRA_OPTION_CUSTOM).
   lwip_default_hooks.h dichiara lwip_dhcp_on_extra_option(struct dhcp*, ...) PRIMA di "struct dhcp;": il tipo resta locale alla dichiarazione
   e lwip_default_hooks.c non compila ("incompatible pointer type"). Dichiarare struct dhcp prima di tutto, solo per il componente lwip, basta. */
#ifndef VOS_LWIP_DHCP_FIX_H
#define VOS_LWIP_DHCP_FIX_H
struct dhcp;
#endif
