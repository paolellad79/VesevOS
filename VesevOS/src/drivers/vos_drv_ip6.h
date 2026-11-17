// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_ip6.h
// STRATO 1 (driver): indirizzi IPv6 dell'interfaccia client (Wi-Fi di casa) con esp_netif di ESP-IDF 5.5.
// Legge gli indirizzi, crea l'indirizzo locale, aggiunge o toglie UN indirizzo fisso. Non decide quando farlo: lo decide net/vos_net_ip6.
// Gli indirizzi sono testo ("2001:db8::1"). Il gateway e i DNS IPv6 non hanno una funzione di impostazione in esp_netif: arrivano dal router.
// Senza CONFIG_LWIP_IPV6 (recovery) le funzioni esistono ma non fanno nulla.
#pragma once
#include <stdint.h>
#include <stddef.h>

#define DRV_IP6_MAX    8      // indirizzi letti al massimo (CONFIG_LWIP_IPV6_NUM_ADDRESSES e 8 nelle nostre build)
#define DRV_IP6_TESTO  40     // "xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx" + 0

enum DrvIp6Tipo : uint8_t { DI6T_LOCALE = 0, DI6T_GLOBALE, DI6T_PRIVATO, DI6T_ALTRO };
enum DrvIp6Esito : uint8_t { DI6E_OK = 0, DI6E_NON_VALIDO, DI6E_NON_AMMESSO };

struct DrvIp6Voce {
  char        testo[DRV_IP6_TESTO];
  DrvIp6Tipo  tipo;
  bool        preferito;     // false = in prova (controllo duplicati) o in via di scadenza
};

void        drvIp6Normale(const uint8_t b[16], char* out, size_t n);   // 16 byte -> testo in forma breve
bool        drvIp6Presente();                                  // il firmware ha IPv6?
bool        drvIp6CreaLocale();                                // crea l'indirizzo locale (serve la Wi-Fi collegata); false = non ancora possibile
int         drvIp6Elenca(DrvIp6Voce* out, int max);            // gli indirizzi validi dell'interfaccia client; ritorna quanti
DrvIp6Esito drvIp6Controlla(const char* in, char* norm, size_t n);   // solo globali o privati (fc00::/7): niente ::, ::1, multicast, locali, IPv4. norm = forma normale
bool        drvIp6Aggiungi(const char* norm);                  // aggiunge un indirizzo (prefisso fisso /64)
bool        drvIp6Togli(const char* norm);
void        drvIp6TogliTutti();

// Chi sta dall'altra parte di un socket TCP, come numero IPv4 (primo numero in alto). Un client IPv4 (anche arrivato su un socket IPv6,
// "mappato") da il suo indirizzo. Un client IPv6 nativo NON ha un indirizzo IPv4: da un indirizzo finto in 240.0.0.0/4 (classe E, mai di un
// client IPv4 vero) ricavato dalla sua rete /64, cosi il blocco per tentativi sbagliati conta per rete e il filtro IP lo riconosce (drvIp6Finto).
uint32_t    drvSockPeer(int sock);
bool        drvIp6Finto(uint32_t ip);                          // l'indirizzo e uno di quelli finti?                                // toglie ogni indirizzo (anche l'indirizzo locale)
