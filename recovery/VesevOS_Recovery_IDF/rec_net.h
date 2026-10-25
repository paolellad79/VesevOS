// VesevOS Recovery - rete: client Wi-Fi (rete di casa, dalla configurazione) oppure hotspot di emergenza "VesevOS-recovery".
// Nome in rete (DHCP e mDNS): lo stesso nome host di VesevOS (vedi recNetName in rec_config.h).
#pragma once
#include <string>

void recNetStart();                  // si collega alla rete di casa (fino a 30 s); se non ci riesce apre l'hotspot. Aspetta muovendo il LED.
bool recNetUp();                     // la rete e stata avviata
bool recNetIsAp();                   // siamo in hotspot
std::string recNetIp();
void recNetInfo();                   // dove aprire la pagina (sulla seriale)
