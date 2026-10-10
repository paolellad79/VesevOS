// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell.cpp
#include "vos_shell.h"
#include "vos_shell_int.h"
String shx::argAt(const String& s, int idx) {
  int n = 0, i = 0;
  while (i < (int)s.length()) {
    while (i < (int)s.length() && s[i] == ' ') i++;
    int st = i;
    while (i < (int)s.length() && s[i] != ' ') i++;
    if (i > st) { if (n == idx) return s.substring(st, i); n++; }
  }
  return "";
}

String shx::restFrom(const String& s, int idx) {
  int n = 0, i = 0;
  while (i < (int)s.length()) {
    while (i < (int)s.length() && s[i] == ' ') i++;
    if (i >= (int)s.length()) break;
    if (n == idx) return s.substring(i);
    while (i < (int)s.length() && s[i] != ' ') i++;
    n++;
  }
  return "";
}

void shx::cmdHelp(Print& o) {
  o.println(tr("Comandi:"));
  o.println(tr("  help            questo elenco"));
  o.println(tr("  uname           versione sistema"));
  o.println(tr("  about           chi siamo e indirizzo GitHub"));
  o.println(tr("  serial-auth [on|off]  password sulla seriale: mostra o cambia"));
  o.println(tr("  serial [baud <n>|eol crlf|lf|cr|echo|input|log|banner on|off|tx <ms>|keep]  impostazioni della seriale"));
  o.println(tr("  uptime          da quanto e acceso (+ motivo reset, avvii)"));
  o.println(tr("  free [detail]   memoria RAM/PSRAM (detail: blocchi e frammentazione)"));
  o.println(tr("  ram [mark|diff|audit] stack dei task e costo dei servizi (audit: storico)"));
  o.println(tr("  events [n]      ultimi eventi del sistema (Event Bus): cambi di stato dei servizi"));
  o.println(tr("  df              spazio su flash"));
  o.println(tr("  temp            temperatura CPU"));
  o.println(tr("  top             CPU, RAM, temperatura"));
  o.println(tr("  ps              elenco task"));
  o.println(tr("  kill <nome>     ferma un task (solo quelli consentiti, vedi ps)"));
  o.println(tr("  start <nome>    avvia o riavvia un task fermato (restart = uguale)"));
  o.println(tr("  rules [run <n>] automazioni: elenco, oppure esegui la regola n"));
  o.println(tr("  boot-order [lista|reset]  ordine di avvio dei servizi"));
  o.println(tr("  ls [cartella]   elenco file (flash)"));
  o.println(tr("  cat <file>      mostra un file"));
  o.println(tr("  mkdir <nome>    crea cartella"));
  o.println(tr("  rm <nome>       elimina file o cartella"));
  o.println(tr("  mv <da> <a>     sposta o rinomina"));
  o.println(tr("  cp <da> <a>     copia un file"));
  o.println(tr("  write <file> <testo>   scrive un file di testo"));
  o.println(tr("  date [set AAAA-MM-GG HH:MM]  data e ora (mostra o imposta)"));
  o.println(tr("  ntp [on|off|sync|every <min>]  stato NTP / accendi, spegni / risincronizza / frequenza"));
  o.println(tr("  cpu [auto|80|160|240]  velocita CPU"));
  o.println(tr("  hostname [nome] mostra o cambia il nome host"));
  o.println(tr("  domain [nome|-] mostra, cambia o toglie il dominio"));
  o.println(tr("  info            riassunto: versione, chip, memoria, rete, MAC"));
  o.println(tr("  net             rete: IP, MAC client e AP, segnale"));
  o.println(tr("  ip              rete e indirizzo"));
  o.println(tr("  wifi            stato wi-fi"));
  o.println(tr("  wifi-scan       cerca reti"));
  o.println(tr("  wifi set <rete> [password] | wifi off   collega la Wi-Fi di casa (nome con spazi tra virgolette) o la scollega"));
  o.println(tr("  setup           configurazione guidata da seriale (lingua, paese, nome, antenna, hotspot, utente, Wi-Fi, ora)"));
  o.println(tr("  setup done      segna la guida come finita (scheda gia in uso)"));
  o.println(tr("  diag            diagnosi: guida, salvataggio, rete, servizi, RAM, flash"));
  o.println(tr("  svc [ap|captive|http|https|dhcp|mdns on|off] [http-port|https-port <n>]  servizi di rete e porte (web e porte dal riavvio, DHCP e mDNS subito)"));
  o.println(tr("  mqtt [start|stop|restart|pub]  servizio MQTT"));
  o.println(tr("  airplane [on <come>|off]  modalita aereo (come: boot, 30s, 10m, 2h, 07:30, fisso)"));
  o.println(tr("  led <modo>      state|heartbeat|fixed|off"));
  o.println(tr("  led-color <RRGGBB>   colore (esadecimale)"));
  o.println(tr("  led-bright <0-255>   luminosita"));
  o.println(tr("  lang [codice]   mostra o cambia la lingua (it, en, ...)"));
  o.println(tr("  license [id]    note legali e licenze (notice, gpl3, lgpl21, apache2, mit, bsd3)"));
  o.println(tr("  dev [status <nome>|on <nome>|off <nome>|act <nome> <azione> [arg]]  periferiche (MQTT, Bluetooth, pin, LED, memoria...)"));
  o.println(tr("  pins            pin usati"));
  o.println(tr("  pin <n> [high|low|blink|read [up|down]|off]  prova un pin (si spegne da solo)"));
  o.println(tr("  config          mostra configurazione (senza password)"));
  o.println(tr("  passwd <utente> <nuova>  cambia la password di un utente (min 6 caratteri)"));
  o.println(tr("  user [add <nome> <admin|oper|guest> <password>|del <nome>|role <nome> <ruolo>|on <nome>|off <nome>]  utenti"));
  o.println(tr("  ap [show|new]   hotspot: mostra la password o creane una nuova"));
  o.println(tr("  locale [country XX|tx <dBm>|antenna int|ext <dBi>]  paese, potenza, antenna"));
  o.println(tr("  firewall [off|confirm|mode <0-3>|add <ip|a-b|rete/nn> [nome]|del <n>|test <ip>]  filtro IP"));
  o.println(tr("  audit [ack [n]] allarmi della configurazione e registro modifiche"));
  o.println(tr("  watchdog        stato del watchdog"));
  o.println(tr("  mesh [start|stop|send <scheda|*> <testo>|cmd <scheda> <azione>|key new|role <0|1|2>]  rete tra schede"));
  o.println(tr("  ble [on [min]|off]  Bluetooth per telefoni e app (senza limite di tempo, o per min minuti)"));
  o.println(tr("  cert [new]      certificato HTTPS: impronta o nuovo certificato"));
  o.println(tr("  legal           note legali: licenze, dati salvati, sicurezza"));
  o.println(tr("  welcome         schermata di benvenuto"));
  o.println(tr("  ban             indirizzi bloccati o sospetti"));
  o.println(tr("  unban <ip|all>  sblocca un indirizzo (o tutti)"));
  o.println(tr("  logout          esce (solo seriale)"));
  o.println(tr("  log [n|clear|level [0-3]]   ultime righe del registro, svuota, o livello (0 errori ... 3 dettagli)"));
  o.println(tr("  reboots [clear]   diario degli ultimi 20 riavvii (motivo, durata, RAM minima) o azzera"));
  o.println(tr("  factory-reset   azzera tutto (poi riavvia)"));
  o.println(tr("  reboot          riavvia"));
  o.println(tr("  recovery [now]  recovery (aggiornamento firmware): stato, o 'now' = riavvia nel recovery"));
  o.println(tr("  sleep [minuti]  sonno profondo (con i minuti la scheda riparte da sola; senza, solo con RESET)"));
  o.println(tr("  stats [on|off|reset]  statistiche locali (spente di fabbrica)"));
  o.println(tr("  selftest [active] [names]  autodiagnosi con report (prove attive: LED e MQTT; names: nomi reali)"));
  o.println(tr("  power [off|wifi|cycle <sveglia> <sonno>]  risparmio energia (minuti)"));
}

String shx::hex6(uint32_t c) {
  char b[8]; snprintf(b, sizeof(b), "%06lX", (unsigned long)(c & 0xFFFFFF)); return String(b);
}

// comandi permessi all'Operatore (uso della scheda, niente rete, sicurezza, utenti, file di sistema)
static bool operOk(const String& c, const String& a1) {
  static const char* const OK[] = {"help", "?", "uname", "about", "uptime", "free", "df", "temp", "top", "ps", "info", "net", "ip", "wifi",
                                   "date", "led", "led-color", "led-bright", "pin", "pins", "rules", "audit", "watchdog", "legal", "license", "licenza", "locale", "dev", "events"};
  for (size_t i = 0; i < sizeof(OK) / sizeof(OK[0]); i++) if (c == OK[i]) return true;
  if (c == "ntp") return a1 == "" || a1 == "sync";
  if (c == "log") return a1 != "clear" && a1 != "level";
  if (c == "reboots") return a1 != "clear";
  if (c == "mqtt") return a1 == "" || a1 == "status" || a1 == "pub";
  if (c == "mesh") return a1 == "" || a1 == "status" || a1 == "send" || a1 == "cmd";
  if (c == "airplane" || c == "aereo" || c == "reboot" || c == "sleep") return true;
  return false;
}

int shx::roleArg(const String& r) { return r == "admin" ? ROLE_ADMIN : r == "oper" ? ROLE_OPER : r == "guest" ? ROLE_GUEST : -1; }

void shellExec(const String& lineIn, Print& o, int role) {
  String line = lineIn; line.trim();
  if (line.length() == 0) return;
  String c = argAt(line, 0);
  c.toLowerCase();
  String a1 = argAt(line, 1);

  if (role < 0 && authIsSet()) { o.println(tr("Non autorizzato. Fai login.")); return; }
  if (role < 0) role = ROLE_ADMIN;                       // nessuna password ancora: primo avvio
  if (role < ROLE_ADMIN && (role < ROLE_OPER || !operOk(c, a1))) { o.println(tr("Il tuo ruolo non permette questo comando")); return; }

  if (shCmdSys(c, a1, line, o, role)) return;
  if (shCmdNet(c, a1, line, o, role)) return;
  if (shCmdAdmin(c, a1, line, o, role)) return;
  o.println(tr("Comando sconosciuto. Scrivi 'help'."));
}

// ---------- schermata di benvenuto (solo ASCII, 68 colonne) ----------
#define WW 68
static void wBar(Print& o) { String b = "+"; for (int i = 0; i < WW - 2; i++) b += '-'; o.println(b + "+"); }
static void wLine(Print& o, const String& t) {
  String x = t; if (x.length() > WW - 4) x = x.substring(0, WW - 4);
  String l = "|  " + x; while (l.length() < WW - 1) l += ' ';
  o.println(l + "|");
}
// testo lungo: a capo sulle parole
static void wPara(Print& o, const String& t) {
  String rest = t;
  while (rest.length()) {
    if ((int)rest.length() <= WW - 4) { wLine(o, rest); break; }
    int cut = rest.lastIndexOf(' ', WW - 4); if (cut <= 0) cut = WW - 4;
    wLine(o, rest.substring(0, cut)); rest = rest.substring(cut + 1);
  }
}

static String g_lcodes[9], g_lnames[9];
static int g_ln = 0;

static void langMenu(Print& o) {
  g_ln = langList(g_lcodes, g_lnames, 9);
  String l = String(tr("Lingua:")) + " ";
  for (int i = 0; i < g_ln; i++) l += " [" + String(i + 1) + "] " + g_lnames[i] + (g_lcodes[i] == cfg.lang ? " *" : "") + " ";
  l += String(" ") + tr("(premi il numero)");
  wPara(o, l);
}

void shellWelcome(Print& o, bool full) {
  o.println();
  wBar(o);
  { String l = "|   /\\      " + String(VOS_NAME) + " " + VOS_VERSION; while (l.length() < WW - 1) l += ' '; o.println(l + "|"); }
  { String l = "|  /  \\     " + String(VOS_BOARD) + " - 4 MB flash, 2 MB PSRAM"; while (l.length() < WW - 1) l += ' '; o.println(l + "|"); }
  { String l = "| /____\\    github.com/paolellad79/VesevOS"; while (l.length() < WW - 1) l += ' '; o.println(l + "|"); }
  wBar(o);
  if (full) {
    wLine(o, tr("BENVENUTO! Questa scheda non e ancora configurata."));
    wLine(o, "");
    wLine(o, tr("1. Collegati alla rete Wi-Fi:"));
    wLine(o, String("     ") + tr("Nome:") + "      " + cfg.apSsid);
    wLine(o, String("     ") + tr("Password:") + "  " + cfg.apPass);
    wLine(o, String(tr("2. Apri:")) + "      http://192.168.4.1");
    wLine(o, String("   ") + tr("(di solito la pagina si apre da sola)"));
    wLine(o, tr("3. Segui la configurazione guidata."));
    wLine(o, "");
    wLine(o, tr("Il QR per il telefono e nella pagina Home."));
    wBar(o);
    wPara(o, tr("Perche una password diversa per ogni scheda? Lo chiedono le leggi sulla sicurezza dei dispositivi: UE (RED/EN 18031, Cyber Resilience Act) e Regno Unito (PSTI). Niente password uguali."));
    wBar(o);
    wLine(o, tr("Password persa?  Tieni premuto BOOT 8 s  -> la riscrivo qui"));
    wLine(o, tr("Da capo?         Tieni premuto BOOT 20 s -> fabbrica"));
    wLine(o, tr("Aiuto: http://192.168.4.1/#aiuto   Comandi: scrivi  help"));
  } else {
    wLine(o, String(VOS_NAME) + " " + VOS_VERSION + "  -  " + cfg.hostname + " (" + cfg.hostname + ".local)");
    if (netState() == NET_CLIENT_OK) wLine(o, trf("Wi-Fi casa: %s  %s  %d dBm", drvWifiSsid().c_str(), netIpString().c_str(), (int)drvWifiRssi()));
    else wLine(o, trf("Hotspot: %s  %s", cfg.apSsid.c_str(), netIpString().c_str()));
    wLine(o, trf("Paese: %s   Ora: %s", cfg.country.length() ? cfg.country.c_str() : "--", timeNowStr().c_str()));
    int red = auditCount(AUD_RED), yel = auditCount(AUD_YELLOW);
    wLine(o, (red || yel) ? trf("Allarmi: %d rossi, %d gialli -> scrivi audit", red, yel) : String(tr("Allarmi: nessuno")));
    wLine(o, trf("Avvio: %s (n. %lu)", sysResetReason().c_str(), (unsigned long)sysBootCount()));
    if (cfg.https && tlsFingerprint().length()) wPara(o, String(tr("Impronta HTTPS:")) + " " + tlsFingerprint().substring(0, 47) + "...");
    wLine(o, trf("Pagina: https://%s   Comandi: help", netIpString().c_str()));
  }
  wBar(o);
  langMenu(o);
  wBar(o);
}

// ---------- shell seriale ----------
static String g_buf;
static uint8_t g_esc = 0;                          // dentro una sequenza ESC (tasti freccia...)
static bool g_fullWelcome = false;

// La seriale chiede la password solo se: password impostata, interruttore acceso, non ancora autenticata
static bool serialLocked() { return authIsSet() && cfg.serialAuth && !serialAuthed(); }

static void prompt() { serOut().print(serialLocked() ? "password: " : "vesevos> "); }

void shellSerialShowPass() { g_fullWelcome = true; }   // tasto BOOT 8 s: rimostra la password dell'hotspot

// ---- configurazione guidata da seriale (stessi passi della pagina web) ----
enum { WZ_OFF = -1, WZ_LANG = 0, WZ_COUNTRY, WZ_NAME, WZ_ANT, WZ_ANTG, WZ_AP, WZ_PASS, WZ_WIFI, WZ_SCAN, WZ_WPASS, WZ_TIME };
static int g_wz = WZ_OFF;
static String g_wzNets[8];
static int g_wzN = 0;
static String g_wzSsid, g_wzPass;
static bool g_wzWifi = false;

static void wzAsk(Print& o) {
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

static void wzNext(Print& o);
static void wzFinish(Print& o) {
  g_wz = WZ_OFF;
  cfg.ntpOn = g_wzWifi; cfg.ntpAutoOff = !g_wzWifi;   // senza Wi-Fi di casa il server dell'ora non serve (si riaccende se arriva il Wi-Fi)
  if (g_wzWifi) { cfg.staEnabled = true; cfg.staSsid = g_wzSsid; cfg.staPass = g_wzPass; cfg.staDhcp = true; }
  cfg.setupDone = true; ledSetSetup(false);
  cfgSetOrigin("seriale setup"); cfgSave(); timeApply();
  o.println(tr("Configurazione finita."));
  o.println(trf("Nome: %s.local   Paese: %s   Lingua: %s", cfg.hostname.c_str(), cfg.country.c_str(), cfg.lang.c_str()));
  if (g_wzWifi) { o.println(trf("Mi collego a %s: se non riesce l'hotspot torna da solo.", g_wzSsid.c_str())); netReconfigure(); }
  else o.println(tr("Senza Wi-Fi di casa: hotspot e HTTP restano accesi, il server dell'ora e spento."));
  o.println(tr("Per i servizi di rete e le porte: svc. Per tutto il resto: help."));
}

static void wzNext(Print& o) {
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
    netScanStart(); delay(300);
    for (int i = 0; i < 30 && netScanJson().indexOf("\"running\":true") >= 0; i++) delay(300);
    String j = netScanJson(); g_wzN = 0;
    int p = 0;
    while (g_wzN < 8) {
      int a = j.indexOf("\"ssid\":\"", p); if (a < 0) break;
      a += 8; int b = j.indexOf("\",\"rssi\"", a); if (b < 0) break;
      String sd = j.substring(a, b); p = b;
      if (sd.length() && sd.indexOf('\\') < 0) { bool dup = false; for (int k = 0; k < g_wzN; k++) if (g_wzNets[k] == sd) dup = true; if (!dup) g_wzNets[g_wzN++] = sd; }
    }
    for (int i = 0; i < g_wzN; i++) o.println(String(i + 1) + ") " + g_wzNets[i]);
    if (!g_wzN) o.println(tr("Nessuna rete trovata: scrivi il nome a mano."));
  }
  wzAsk(o);
}

void shx::wzStart(Print& o) {
  if (!serIsOut(o)) { o.println(tr("La configurazione guidata si fa solo dalla seriale (USB).")); return; }
  g_wz = WZ_LANG; g_wzWifi = false; g_wzSsid = ""; g_wzPass = "";
  o.println(tr("Configurazione guidata da seriale. Invio = tieni il valore proposto, skip = salta il passo, quit = esci."));
  wzAsk(o);
}

static void wzLine(Print& o, String l) {
  l.trim();
  if (l == "quit") { g_wz = WZ_OFF; o.println(tr("Configurazione interrotta: la guida resta da finire (scrivi setup per riprendere).")); return; }
  String err;
  bool skip = (l == "skip" || l.length() == 0);
  switch (g_wz) {
    case WZ_LANG: if (!skip) { l.toLowerCase(); if (!langSet(l, err)) { o.println(err); wzAsk(o); return; } } break;
    case WZ_COUNTRY: if (!skip) { l.toUpperCase(); if (!regionValid(l)) { o.println(tr("Paese sconosciuto")); wzAsk(o); return; } cfg.country = l; cfgSave(); regionApplyRadio(); } break;
    case WZ_NAME: if (!skip) { l.toLowerCase(); if (!hostnameValid(l)) { o.println(tr("Nome non valido (1-32: lettere, numeri, trattino)")); wzAsk(o); return; } cfg.hostname = l; cfgSave(); } break;
    case WZ_ANT:
      if (l == "e") { cfg.antExt = 1; g_wz = WZ_ANTG; wzAsk(o); return; }
      if (!skip && l != "i") { o.println(tr("Scrivi i oppure e")); wzAsk(o); return; }
      cfg.antExt = 0; cfg.antGain = 0; cfgSave(); regionApplyRadio(); break;
    case WZ_ANTG: { int g = skip ? 0 : l.toInt(); if (g < 0 || g > 15) { o.println(tr("Guadagno dell'antenna: da 0 a 15 dBi")); wzAsk(o); return; } cfg.antGain = g; cfgSave(); regionApplyRadio(); } break;
    case WZ_AP: if (l == "n") { cfg.apPass = cfgNewApPass(); cfgSave(); o.println(trf("Nuova password hotspot: %s", cfg.apPass.c_str())); } break;
    case WZ_PASS:
      if (!skip) {
        int a = authFirstAdmin();
        if (a < 0 || !userSetPassword(a, l, err)) { o.println(err.length() ? err : String(tr("Errore"))); wzAsk(o); return; }
        cfg.users[a].on = true; cfg.users[a].role = ROLE_ADMIN; cfgSave();
      } else if (!authIsSet()) { o.println(tr("Serve una password per l'amministratore.")); wzAsk(o); return; }
      break;
    case WZ_WIFI: g_wzWifi = (l == "s" || l == "S" || l == "si"); break;
    case WZ_SCAN: {
      if (skip) { g_wzWifi = false; g_wz = WZ_TIME; wzAsk(o); return; }
      int n = l.toInt();
      g_wzSsid = (n >= 1 && n <= g_wzN && String(n) == l) ? g_wzNets[n - 1] : l;
      if (g_wzSsid.length() > 32) { o.println(tr("Nome rete non valido")); wzAsk(o); return; }
    } break;
    case WZ_WPASS:
      if (l.length() && (l.length() < 8 || l.length() > 63)) { o.println(tr("Password Wi-Fi: da 8 a 63 caratteri")); wzAsk(o); return; }
      g_wzPass = l; break;
    case WZ_TIME:
      if (!skip) { if (!timeSetLocal(l, err)) { o.println(err); wzAsk(o); return; } o.println(timeNowStr()); }
      break;
  }
  wzNext(o);
}

bool shellWizardActive() { return g_wz != WZ_OFF; }

void shellSerialPoll() {
  static bool first = true, wasOpen = false;
  static uint32_t lastBanner = 0, lastKey = 0, closedAt = 0;
  static bool closedLong = true;
  uint32_t now = millis();
  bool open = (bool)Serial;                       // true quando il monitor seriale e aperto sul PC
  // anti-ripetizione: il segnale "aperto" sull'USB puo cadere e risalire piu volte in pochi decimi di secondo
  // (monitor che si riapre, velocita diversa): conta come nuova apertura solo dopo 2 secondi di chiusura vera
  if (!open) { if (!closedAt) closedAt = now ? now : 1; else if (now - closedAt >= 2000UL) closedLong = true; }
  else closedAt = 0;
  bool show = first || g_fullWelcome;
  if (!cfg.setupDone && open) {
    // finche la configurazione guidata non e finita il benvenuto (con la password dell'hotspot) NON sparisce:
    // si rimostra a ogni apertura del monitor e, se nessuno scrive, ogni 30 secondi
    if (!wasOpen && closedLong && (lastBanner == 0 || now - lastBanner >= 5000UL)) show = true;
    else if (now - lastBanner >= 30000UL && now - lastKey >= 30000UL) show = true;
  }
  if (open && !wasOpen) closedLong = false;
  wasOpen = open;
  if (show) {
    bool full = !cfg.setupDone || g_fullWelcome;
    first = false; g_fullWelcome = false; if (open) lastBanner = now;   // il benvenuto mostrato a monitor chiuso non conta
    if (cfg.serBanner || !cfg.setupDone) shellWelcome(serOut(), full);
    if (!authIsSet()) serOut().println(tr("Nessuna password: impostala dalla pagina web (primo accesso)."));
    if (!cfg.setupDone) { serOut().println(tr("Configurazione non finita: questo messaggio resta finche la guida non e completata (pagina web oppure seriale).")); serOut().println(tr("Per configurare da questa seriale scrivi: setup")); }
    prompt();
  }
  while (Serial.available()) {
    char ch = Serial.read();
    if (!cfg.serIn) continue;                      // seriale impostata "senza comandi": si butta quello che arriva
    lastKey = now;
    if (g_esc || ch == 27) { String e; utilEditKey(g_buf, g_esc, ch, e, 200); continue; }   // frecce e simili: scartate, non finiscono nella riga
    // tasto numerico a riga vuota: cambia lingua (solo con la seriale sbloccata)
    if (!g_buf.length() && ch >= '1' && ch <= '9' && !serialLocked() && g_wz == WZ_OFF) {
      int i = ch - '1';
      if (!g_ln) g_ln = langList(g_lcodes, g_lnames, 9);
      if (i < g_ln) {
        String err;
        if (langSet(g_lcodes[i], err)) { serOut().println(); serOut().println(trf("Lingua impostata: %s", g_lnames[i].c_str())); shellWelcome(serOut(), !cfg.setupDone); }
        else serOut().println(err);
        prompt();
        continue;
      }
    }
    if (ch == '\r' || ch == '\n') {
      String l = g_buf; g_buf = "";
      if (l.length() == 0 && ch == '\n') continue;
      serOut().println();
      if (g_wz != WZ_OFF && !serialLocked()) { cfgSetOrigin("seriale"); wzLine(serOut(), l); prompt(); continue; }   // risposta alla guida
      if (l.length() == 0 && !cfg.setupDone) { g_fullWelcome = true; continue; }   // Invio a vuoto: rimostra il benvenuto
      if (serialLocked()) {
        if (authLocked()) serOut().println(tr("Bloccato per troppi errori, riprova tra un minuto."));
        else if (authCheck(l)) { serialAuthSet(true); serOut().println(tr("Accesso eseguito.")); }
        else serOut().println(tr("Password errata."));
      } else if (l == "logout") { serialAuthSet(false); serOut().println(tr("Uscito.")); }
      else { cfgSetOrigin("seriale"); shellExec(l, serOut(), ROLE_ADMIN); }
      prompt();
    } else {
      String e; utilEditKey(g_buf, g_esc, ch, e, 200);          // Backspace, Ctrl+U, Ctrl+C e lettere
      if (e.length() && !serialLocked() && cfg.serEcho) serOut().print(e);   // niente eco della password; "\b \b" cancella anche sullo schermo
    }
  }
}
