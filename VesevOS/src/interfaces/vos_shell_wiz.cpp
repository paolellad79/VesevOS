// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_wiz.cpp
// Configurazione guidata da seriale (comando "setup"): stessi passi della pagina web. Le risposte arrivano come testo semplice.
#include "vos_shell_int.h"
#include "vos_shell_login.h"
#include "../drivers/vos_drv_time.h"
#include <string_view>

enum { WZ_OFF = -1, WZ_LANG = 0, WZ_COUNTRY, WZ_NAME, WZ_ANT, WZ_ANTG, WZ_AP, WZ_PASS, WZ_WIFI, WZ_SCAN, WZ_WPASS, WZ_TIME };
#define WZ_NETS 8
#define WZ_SSID_MAX 32
#define WZ_PASS_MAX 63
static int  g_wz = WZ_OFF;
static char g_wzNets[WZ_NETS][WZ_SSID_MAX + 1];
static int  g_wzN = 0;
static char g_wzSsid[WZ_SSID_MAX + 1], g_wzPass[WZ_PASS_MAX + 1];
static bool g_wzWifi = false;

static bool same(const char* a, const char* b) { return strcmp(a, b) == 0; }

static void wzAsk(OutSink& o) {
  switch (g_wz) {
    case WZ_LANG: o.println(trf("1/8 Lingua [%s] (it, en, ...):", cfg.lang.c_str())); break;
    case WZ_COUNTRY: o.println(trf("2/8 Paese, codice di 2 lettere (es. IT) [%s]:", cfg.country.length() ? cfg.country.c_str() : "-")); break;
    case WZ_NAME: o.println(trf("3/8 Nome della scheda [%s]:", cfg.hostname.c_str())); break;
    case WZ_ANT: o.println(tr("4/8 Antenna: i = interna, e = esterna [i]:")); break;
    case WZ_ANTG: o.println(tr("Guadagno dell'antenna esterna in dBi (0-15) [0]:")); break;
    case WZ_AP: o.println(trf("5/8 Hotspot: %s  password: %s", cfg.apSsid.c_str(), cfg.apPass.c_str())); o.println(tr("Invio = tieni, n = nuova password casuale:")); break;
    case WZ_PASS:
      if (authIsSet()) o.println(tr("6/8 Amministratore gia impostato. Invio = tieni, oppure scrivi la nuova password:"));
      else o.println(trf("6/8 Scegli la password dell'amministratore '%s' (almeno 6 caratteri; si vede mentre scrivi: fallo in un luogo sicuro):", cfg.users[authFirstAdmin() < 0 ? 0 : authFirstAdmin()].name.c_str()));
      break;
    case WZ_WIFI: o.println(tr("7/8 Collegare la Wi-Fi di casa? s = si, Invio = no (hotspot e HTTP restano accesi):")); break;
    case WZ_SCAN: o.println(tr("Numero della rete, oppure scrivi il nome:")); break;
    case WZ_WPASS: o.println(tr("Password della Wi-Fi (Invio = rete aperta):")); break;
    case WZ_TIME: o.println(tr("8/8 Ora a mano, AAAA-MM-GG HH:MM (Invio = salta):")); break;
  }
}


static void wzNext(OutSink& o);
static void wzFinish(OutSink& o) {
  g_wz = WZ_OFF;
  cfg.ntpOn = g_wzWifi; cfg.ntpAutoOff = !g_wzWifi;   // senza Wi-Fi di casa il server dell'ora non serve (si riaccende se arriva il Wi-Fi)
  if (g_wzWifi) { cfg.staEnabled = true; cfg.staSsid = g_wzSsid; cfg.staPass = g_wzPass; cfg.staDhcp = true; }
  cfg.setupDone = true; ledSetSetup(false);
  cfgSetOrigin("seriale setup"); cfgSave(); timeApply();
  o.println(tr("Configurazione finita."));
  o.println(trf("Nome: %s.local   Paese: %s   Lingua: %s", cfg.hostname.c_str(), cfg.country.c_str(), cfg.lang.c_str()));
  if (g_wzWifi) { o.println(trf("Mi collego a %s: se non riesce l'hotspot torna da solo.", g_wzSsid)); netReconfigure(); }
  else o.println(tr("Senza Wi-Fi di casa: hotspot e HTTP restano accesi, il server dell'ora e spento."));
  o.println(tr("Per i servizi di rete e le porte: svc. Per tutto il resto: help."));
}

// Reti trovate dalla scansione: dal JSON {"ssid":"..","rssi":..} prende i nomi semplici (senza barra rovesciata), senza doppioni.
static void wzReadNets(std::string_view j) {
  g_wzN = 0;
  size_t p = 0;
  while (g_wzN < WZ_NETS) {
    size_t a = j.find("\"ssid\":\"", p); if (a == std::string_view::npos) break;
    a += 8;
    size_t b = j.find("\",\"rssi\"", a); if (b == std::string_view::npos) break;
    std::string_view sd = j.substr(a, b - a);
    p = b;
    if (sd.empty() || sd.size() > WZ_SSID_MAX || sd.find('\\') != std::string_view::npos) continue;
    bool dup = false;
    for (int k = 0; k < g_wzN; k++) if (sd == g_wzNets[k]) dup = true;
    if (dup) continue;
    memcpy(g_wzNets[g_wzN], sd.data(), sd.size());
    g_wzNets[g_wzN][sd.size()] = 0;
    g_wzN++;
  }
}

static void wzNext(OutSink& o) {
  switch (g_wz) {
    case WZ_LANG: g_wz = WZ_COUNTRY; break;
    case WZ_COUNTRY: g_wz = WZ_NAME; break;
    case WZ_NAME: g_wz = WZ_ANT; break;
    case WZ_ANT: g_wz = WZ_AP; break;
    case WZ_ANTG: g_wz = WZ_AP; break;
    case WZ_AP: g_wz = WZ_PASS; break;
    case WZ_PASS: g_wz = WZ_WIFI; break;
    case WZ_WIFI: g_wz = g_wzWifi ? WZ_SCAN : WZ_TIME; break;
    case WZ_SCAN: g_wz = WZ_WPASS; break;
    case WZ_WPASS: g_wz = WZ_TIME; break;
    case WZ_TIME: wzFinish(o); return;
  }
  if (g_wz == WZ_SCAN) {                      // elenco reti
    netScanStart(); drvDelayMs(300);
    for (int i = 0; i < 30 && scanRunning(); i++) drvDelayMs(300);
    HeapBuf j(NET_SCAN_JSON_MAX);
    if (netScanJson(j)) wzReadNets(std::string_view(j.c_str(), j.length()));
    for (int i = 0; i < g_wzN; i++) o.printf("%d) %s\r\n", i + 1, g_wzNets[i]);
    if (!g_wzN) o.println(tr("Nessuna rete trovata: scrivi il nome a mano."));
  }
  wzAsk(o);
}

void shx::wzStart(OutSink& o) {
  if (!serIsOut(o)) { o.println(tr("La configurazione guidata si fa solo dalla seriale (USB).")); return; }
  g_wz = WZ_LANG; g_wzWifi = false; g_wzSsid[0] = 0; g_wzPass[0] = 0;
  o.println(tr("Configurazione guidata da seriale. Invio = tieni il valore proposto, skip = salta il passo, quit = esci."));
  wzAsk(o);
}

// toglie gli spazi ai lati, sul posto
static void trimSpaces(char* s) {
  size_t n = strlen(s), a = 0;
  while (a < n && s[a] == ' ') a++;
  while (n > a && s[n - 1] == ' ') n--;
  memmove(s, s + a, n - a);
  s[n - a] = 0;
}
static void lowerAscii(char* s) { for (; *s; s++) if (*s >= 'A' && *s <= 'Z') *s = (char)(*s + 32); }
static void upperAscii(char* s) { for (; *s; s++) if (*s >= 'a' && *s <= 'z') *s = (char)(*s - 32); }

// Una risposta della guida. 'l' e la riga scritta (viene modificata: spazi tolti, a volte maiuscole o minuscole).
void shx::wzLine(OutSink& o, char* l) {
  trimSpaces(l);
  if (same(l, "quit")) { g_wz = WZ_OFF; o.println(tr("Configurazione interrotta: la guida resta da finire (scrivi setup per riprendere).")); return; }
  ErrBuf err;
  size_t len = strlen(l);
  bool skip = (same(l, "skip") || len == 0);
  switch (g_wz) {
    case WZ_LANG: if (!skip) { lowerAscii(l); if (!langSet(l, err)) { o.println(err.c_str()); wzAsk(o); return; } } break;
    case WZ_COUNTRY: if (!skip) { upperAscii(l); if (!regionValid(l)) { o.println(tr("Paese sconosciuto")); wzAsk(o); return; } cfg.country = l; cfgSave(); regionApplyRadio(); } break;
    case WZ_NAME: if (!skip) { lowerAscii(l); if (!hostnameValid(l)) { o.println(tr("Nome non valido (1-32: lettere, numeri, trattino)")); wzAsk(o); return; } cfg.hostname = l; cfgSave(); } break;
    case WZ_ANT:
      if (same(l, "e")) { cfg.antExt = 1; g_wz = WZ_ANTG; wzAsk(o); return; }
      if (!skip && !same(l, "i")) { o.println(tr("Scrivi i oppure e")); wzAsk(o); return; }
      cfg.antExt = 0; cfg.antGain = 0; cfgSave(); regionApplyRadio(); break;
    case WZ_ANTG: { int g = skip ? 0 : (int)atol(l); if (g < 0 || g > 15) { o.println(tr("Guadagno dell'antenna: da 0 a 15 dBi")); wzAsk(o); return; } cfg.antGain = g; cfgSave(); regionApplyRadio(); } break;
    case WZ_AP: if (same(l, "n")) { cfg.apPass = cfgNewApPass().c_str(); cfgSave(); o.println(trf("Nuova password hotspot: %s", cfg.apPass.c_str())); } break;
    case WZ_PASS:
      if (!skip) {
        bool firstRun = !authIsSet();
        int a = authFirstAdmin();
        if (a < 0 || !userSetPassword(a, l, err)) { o.println(err.length() ? err.c_str() : tr("Errore")); wzAsk(o); return; }
        cfg.users[a].on = true; cfg.users[a].role = ROLE_ADMIN; cfgSave();
        if (firstRun) serialOpen(a);          // primo avvio: chi ha scelto la prima password resta dentro, la guida non si interrompe
      } else if (!authIsSet()) { o.println(tr("Serve una password per l'amministratore.")); wzAsk(o); return; }
      break;
    case WZ_WIFI: g_wzWifi = (same(l, "s") || same(l, "S") || same(l, "si")); break;
    case WZ_SCAN: {
      if (skip) { g_wzWifi = false; g_wz = WZ_TIME; wzAsk(o); return; }
      int n = (int)atol(l);
      char nTxt[12]; snprintf(nTxt, sizeof nTxt, "%d", n);
      const char* ssid = (n >= 1 && n <= g_wzN && same(nTxt, l)) ? g_wzNets[n - 1] : l;     // un numero scelto dall'elenco, oppure il nome scritto
      if (strlen(ssid) > WZ_SSID_MAX) { o.println(tr("Nome rete non valido")); wzAsk(o); return; }
      strcpy(g_wzSsid, ssid);
    } break;
    case WZ_WPASS:
      if (len && (len < 8 || len > WZ_PASS_MAX)) { o.println(tr("Password Wi-Fi: da 8 a 63 caratteri")); wzAsk(o); return; }
      strcpy(g_wzPass, l); break;
    case WZ_TIME:
      if (!skip) {
        if (!timeSetLocal(l, err)) { o.println(err.c_str()); wzAsk(o); return; }
        char now[32]; timeNowFmt(now, sizeof now); o.println(now);
      }
      break;
  }
  wzNext(o);
}

bool shx::wzActive() { return g_wz != WZ_OFF; }

void shx::wzAbort() {                       // la sessione seriale finisce: la guida si lascia e i dati scritti si cancellano
  g_wz = WZ_OFF;
  wipeBytes(g_wzPass, sizeof g_wzPass);
}
