// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_network.cpp
// Comando "vesevos-network": riepilogo di rete in testo (non JSON) e due scorciatoie.
//
//   vesevos-network [info] [-f]   riepilogo (tutti i ruoli); -f = MAC intero (solo Admin)
//   vesevos-network scan          = "wifi scan"   (solo Admin, come oggi)
//   vesevos-network ping <host>   = "ping <host>" (solo Admin, come oggi)
//
// I comandi ip, net e wifi restano com'erano: questo li riassume, senza JSON grezzo. Il riepilogo legge solo variabili e contatori
// (nessuna scansione, nessuna prova di rete): e veloce e non blocca la pagina. scan e ping non hanno codice qui: ricostruiscono la riga
// del comando che c'e gia e la fanno passare dal PERCORSO NORMALE della shell (shellExecSess), con la stessa tabella dei ruoli.
//
// MAI nel testo: password della Wi-Fi di casa o dell'hotspot, chiavi, impronte. Il MAC e mascherato come nel bugreport (AA:BB:CC:xx:xx:xx).
#include "vos_shell_int.h"
#include "vos_shell_rep.h"
#include "../drivers/vos_drv_activity.h"

namespace {

const size_t NET_OUT_MAX = 3072;       // tetto del riepilogo (di solito ne bastano 1200)

const char* modeText() {
  switch (netState()) {
    case NET_AIR:        return tr("aereo (radio spenta)");
    case NET_CLIENT_OK:  return tr("client (collegata alla Wi-Fi di casa)");
    case NET_CLIENT_TRY: return tr("client (collegamento in corso)");
    case NET_AP:         return tr("hotspot (solo punto di accesso)");
    default:             return tr("avvio");
  }
}

// Qualita del segnale in parole, dal livello in dBm.
const char* qualityWord(int rssi) {
  if (rssi >= -55) return tr("ottimo");
  if (rssi >= -67) return tr("buono");
  if (rssi >= -80) return tr("discreto");
  return tr("debole");
}

// Qualita in percento (stima semplice: -100 dBm = 0, -50 dBm o meglio = 100).
int qualityPercent(int rssi) {
  int q = 2 * (rssi + 100);
  return q < 0 ? 0 : q > 100 ? 100 : q;
}

// MAC mascherato: AA:BB:CC:xx:xx:xx. Con full (solo Admin) intero. Un MAC che non si legge diventa "-".
void macShown(const char* mac, bool full, char* out, size_t n) {
  if (!mac[0]) { snprintf(out, n, "-"); return; }
  if (full || strlen(mac) < 17) snprintf(out, n, "%s", mac);
  else snprintf(out, n, "%.8s:xx:xx:xx", mac);
}

// Byte in forma corta: 512 B, 3.4 KB, 12.0 MB (il contatore e a 32 bit e si riavvolge: vale dall'avvio, in modo approssimato).
void bytesShown(uint32_t b, char* out, size_t n) {
  if (b < 1024UL) snprintf(out, n, "%lu B", (unsigned long)b);
  else if (b < 1024UL * 1024UL) snprintf(out, n, "%lu.%lu KB", (unsigned long)(b / 1024UL), (unsigned long)((b % 1024UL) * 10UL / 1024UL));
  else snprintf(out, n, "%lu.%lu MB", (unsigned long)(b / (1024UL * 1024UL)), (unsigned long)((b % (1024UL * 1024UL)) * 10UL / (1024UL * 1024UL)));
}

// Il nome della rete di casa: quella a cui si e collegati, oppure quella configurata ma non collegata.
void ssidShown(char* out, size_t n) {
  if (netState() == NET_CLIENT_OK) snprintf(out, n, "%s", drvWifiSsid().s);
  else if (cfg.staEnabled && cfg.staSsid.length()) snprintf(out, n, "%s (%s)", cfg.staSsid.c_str(), tr("non collegata"));
  else snprintf(out, n, "%s", tr("nessuna configurata"));
}

void rowDns(HeapBuf& h) {
  char a[16] = "", b[16] = "", line[40];
  if (drvWifiDns(0)) ipTo(drvWifiDns(0), a, sizeof a);
  if (drvWifiDns(1)) ipTo(drvWifiDns(1), b, sizeof b);
  if (!a[0] && !b[0]) snprintf(line, sizeof line, "-");
  else if (a[0] && b[0]) snprintf(line, sizeof line, "%s, %s", a, b);
  else snprintf(line, sizeof line, "%s", a[0] ? a : b);
  repRow(h, TRK("DNS"), line);
}

void rowNtp(HeapBuf& h) {
  if (!cfg.ntpOn) { repRow(h, TRK("NTP"), tr("spento")); return; }
  repRow(h, TRK("NTP"), trf("acceso, server %s, ora %s", cfg.ntpServer.c_str(), timeValid() ? tr("valida") : tr("non valida")).c_str());
}

void sectionHotspot(HeapBuf& h) {
  repHead(h, TRK("Hotspot"));
  repRow(h, TRK("Stato"), repOnOff(cfg.apOn));
  if (!cfg.apOn) return;
  repRow(h, TRK("Nome"), cfg.apSsid.c_str());                       // la password dell'hotspot non si scrive MAI qui
  char n[12];
  snprintf(n, sizeof n, "%d", drvWifiApClients());
  repRow(h, TRK("Collegati"), n);
  repRow(h, TRK("DHCP hotspot"), repOnOff(cfg.dhcpOn));
  repRow(h, TRK("Portale automatico"), repOnOff(netCaptive()));
}

void sectionPorts(HeapBuf& h) {
  repHead(h, TRK("Servizi di rete aperti"));
  char v[64];
  if (cfg.httpOn) snprintf(v, sizeof v, "%s, %s %u (TCP)", tr("aperto"), tr("porta"), (unsigned)cfg.httpPort);
  else snprintf(v, sizeof v, "%s", tr("chiuso"));
  repRaw(h, "HTTP", v);
  if (!cfg.https) snprintf(v, sizeof v, "%s", tr("chiuso"));
  else if (!webHttpsUp()) snprintf(v, sizeof v, "%s", tr("acceso ma non ancora in funzione"));
  else snprintf(v, sizeof v, "%s, %s %u (TCP)", tr("aperto"), tr("porta"), (unsigned)cfg.httpsPort);
  repRaw(h, "HTTPS", v);
  repRow(h, TRK("Redirect"), cfg.httpRedirect && cfg.httpOn && cfg.https ? tr("attivo (HTTP verso HTTPS)") : tr("spento"));
  if (cfg.mdnsOn) snprintf(v, sizeof v, "%s, %s 5353 (UDP)", tr("aperto"), tr("porta"));
  else snprintf(v, sizeof v, "%s", tr("chiuso"));
  repRaw(h, "mDNS", v);
}

void summary(OutSink& o, bool full) {
  HeapBuf h(NET_OUT_MAX);
  repHead(h, TRK("Rete"));
  shNetBrief(h);
  if (netState() == NET_CLIENT_OK) {
    repRow(h, TRK("Maschera"), IpText(drvWifiMask()).s);
    repRow(h, TRK("Gateway"), IpText(drvWifiGateway()).s);
    rowDns(h);
  }
  char v[96];
  if (drvWifiChannel() > 0) { snprintf(v, sizeof v, "%d", drvWifiChannel()); repRow(h, TRK("Canale"), v); }
  if (netState() == NET_CLIENT_OK) {
    int r = drvWifiRssi();
    repRow(h, TRK("Segnale radio"), trf("%d dBm, %d%% (%s)", r, qualityPercent(r), qualityWord(r)).c_str());
    repRow(h, TRK("DHCP client"), cfg.staDhcp ? tr("si, indirizzo automatico") : tr("no, indirizzo fisso"));
  }
  snprintf(v, sizeof v, "%s.local (%s)", cfg.hostname.c_str(), repOnOff(cfg.mdnsOn));
  repRow(h, TRK("Nome host"), cfg.hostname.c_str());
  repRow(h, TRK("Nome mDNS"), v);
  char mac[20], shown[24];
  netMacTo(false, mac, sizeof mac);
  macShown(mac, full, shown, sizeof shown);
  repRow(h, TRK("MAC"), shown);
  if (cfg.apOn) {
    netMacTo(true, mac, sizeof mac);
    macShown(mac, full, shown, sizeof shown);
    repRow(h, TRK("MAC hotspot"), shown);
  }
  rowNtp(h);
  char rx[16], tx[16];
  bytesShown(drvActGet(ACT_WIFI_RX), rx, sizeof rx);
  bytesShown(drvActGet(ACT_WIFI_TX), tx, sizeof tx);
  repRow(h, TRK("Traffico"), trf("ricevuti %s, inviati %s (dall'avvio)", rx, tx).c_str());
  sectionHotspot(h);
  sectionPorts(h);
  if (!full) {
    h.add(tr("(MAC mascherato; l'Admin lo vede intero con -f)"));
    h.add('\n');
  }
  repFlush(o, h);
}

// Ricostruisce la riga di un comando che c'e gia e la fa passare dal percorso normale della shell: ruoli, limiti e conferme sono quelli di sempre.
void forwardLine(const char* cmd, const char* rest, OutSink& o, int role, uint32_t sess) {
  char fwd[256];
  int n = snprintf(fwd, sizeof fwd, "%s %s", cmd, rest);
  if (n < 0 || (size_t)n >= sizeof fwd) { o.println(tr("Comando troppo lungo")); return; }
  shellExecSess(fwd, o, role, sess);
}

}  // namespace

// Le righe brevi: modo, rete Wi-Fi e IP (le usano il riepilogo e vesevos-config).
void shx::shNetBrief(HeapBuf& h) {
  char ip[16], ssid[48];
  repRow(h, TRK("Modo"), modeText());
  ssidShown(ssid, sizeof ssid);
  repRow(h, TRK("Rete Wi-Fi"), ssid);
  netIpTo(ip, sizeof ip);
  repRow(h, TRK("IP"), ip);
}

void shx::shNetworkSummary(OutSink& o, bool full) { summary(o, full); }

void shx::shNetwork(const char* line, OutSink& o, int role, uint32_t sess) {
  bool full = false;
  for (int i = 1;; i++) {
    Word w = argAt(line, i);
    if (w.empty()) break;
    w.lower();
    if (w == "-f") full = true;
    else if (w == "info") continue;
    else if (i == 1 && w == "scan" && argAt(line, 2).empty()) { forwardLine("wifi", "scan", o, role, sess); return; }
    else if (i == 1 && w == "ping" && !argAt(line, 2).empty()) { forwardLine("ping", restFrom(line, 2), o, role, sess); return; }
    else { o.println(tr("Uso: vesevos-network [info] [-f] | scan | ping <host>")); return; }
  }
  if (full && role != ROLE_ADMIN) { o.println(tr("Serve il livello Admin")); return; }
  summary(o, full);
}
