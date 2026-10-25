// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_ping.cpp
// Comando "ping" nello stile di iputils-ping: ping [-c n] [-i secondi] [-W secondi] [-s byte] [-t ttl] host
// Solo Admin (vos_shell_roles.cpp). I LIMITI sono quelli del driver (vos_drv_ping.h): il comando li controlla prima, per dire all'utente
// cosa e sbagliato, e il driver li ricontrolla. Durata totale: al massimo 30 s dalla seriale e 12 s dalla pagina web. Perche: il comando
// BLOCCA chi lo esegue; il server web (httpd) ha uno stack da 8 KB, serve tutte le richieste con un solo task e il Task watchdog e a 30 s.
// Sulla seriale il watchdog si alimenta a ogni giro di attesa (drvTaskPause) e Ctrl+C ferma il ping.
// Le righe di uscita (risposte, "--- statistics ---", "rtt min/avg/max/mdev") sono in inglese come in iputils: restano leggibili dagli
// script. Gli errori e gli avvisi si traducono. Un solo host per volta, solo IPv4, niente broadcast/multicast.
#include "vos_shell_int.h"
#include "../drivers/vos_drv_ping.h"
#include "../drivers/vos_drv_resolve.h"
#include "../drivers/vos_drv_mdns.h"
#include "../drivers/vos_drv_console.h"

namespace {

const uint32_t kDefCount = 4;
const uint32_t kDefIntervalMs = 1000;
const uint32_t kDefTimeoutMs = 2000;
const uint32_t kDefSize = 56;
const uint32_t kDefTtl = 64;
const uint32_t kMaxSerialMs = 30000;       // durata massima dalla seriale
const uint32_t kMaxWebMs = 12000;          // durata massima dalla pagina web (e da script e regole)
const uint32_t kNameTimeoutMs = 2000;      // attesa massima per risolvere un nome (DNS o mDNS)
const uint32_t kIcmpHeader = 8;            // byte dell'intestazione ICMP
const uint32_t kIpHeader = 20;             // byte dell'intestazione IP

struct Opts {
  uint32_t count = kDefCount;
  uint32_t intervalMs = kDefIntervalMs;
  uint32_t timeoutMs = kDefTimeoutMs;
  uint32_t size = kDefSize;
  uint32_t ttl = kDefTtl;
  Word     host;
};

// ---------- lettura dei numeri ----------

// Solo cifre (al massimo 9): niente segno, niente spazi, niente 0x.
bool parseUint(const char* s, uint32_t& out) {
  size_t n = strlen(s);
  if (n == 0 || n > 9) return false;
  uint32_t v = 0;
  for (size_t i = 0; i < n; i++) {
    if (s[i] < '0' || s[i] > '9') return false;
    v = v * 10 + (uint32_t)(s[i] - '0');
  }
  out = v;
  return true;
}

// Secondi (anche con la virgola decimale: 0.2, 1.5) in millisecondi. Al massimo 3 decimali, fino a 99 secondi.
bool parseSeconds(const char* s, uint32_t& ms) {
  uint32_t whole = 0, frac = 0, digits = 0;
  size_t i = 0;
  for (; s[i] >= '0' && s[i] <= '9'; i++) {
    whole = whole * 10 + (uint32_t)(s[i] - '0');
    if (i >= 2) return false;
  }
  if (i == 0) return false;
  if (s[i] == '.') {
    for (i++; s[i] >= '0' && s[i] <= '9'; i++, digits++) {
      if (digits >= 3) return false;
      frac = frac * 10 + (uint32_t)(s[i] - '0');
    }
    if (digits == 0) return false;
    for (uint32_t k = digits; k < 3; k++) frac *= 10;
  }
  if (s[i] != 0) return false;
  ms = whole * 1000 + frac;
  return true;
}

// ---------- opzioni ----------

// Applica una opzione con il suo valore. Ritorna false (dopo aver scritto l'errore) se il valore e fuori dai limiti.
bool setOption(char opt, const char* val, Opts& p, OutSink& o) {
  uint32_t v = 0;
  switch (opt) {
    case 'c':
      if (!parseUint(val, v) || v < 1 || v > DRV_PING_COUNT_MAX) { o.println(trf("ping: -c: da 1 a %u pacchetti", (unsigned)DRV_PING_COUNT_MAX)); return false; }
      p.count = v;
      return true;
    case 'i':
      if (!parseSeconds(val, v) || v < DRV_PING_INTERVAL_MIN_MS || v > DRV_PING_INTERVAL_MAX_MS) { o.println(tr("ping: -i: da 0.2 a 30 secondi (minimo 0.2: niente flood)")); return false; }
      p.intervalMs = v;
      return true;
    case 'W':
      if (!parseSeconds(val, v) || v < DRV_PING_TIMEOUT_MIN_MS || v > DRV_PING_TIMEOUT_MAX_MS) { o.println(tr("ping: -W: da 0.1 a 5 secondi")); return false; }
      p.timeoutMs = v;
      return true;
    case 's':
      if (!parseUint(val, v) || v < DRV_PING_SIZE_MIN || v > DRV_PING_SIZE_MAX) { o.println(trf("ping: -s: da %u a %u byte", (unsigned)DRV_PING_SIZE_MIN, (unsigned)DRV_PING_SIZE_MAX)); return false; }
      p.size = v;
      return true;
    case 't':
      if (!parseUint(val, v) || v < 1 || v > 255) { o.println(tr("ping: -t: da 1 a 255")); return false; }
      p.ttl = v;
      return true;
  }
  return false;
}

// Legge le parole della riga: le opzioni (con il valore attaccato, "-c3", o nella parola dopo, "-c 3") e un solo host.
// Ritorna false se qualcosa non va (l'errore e gia scritto). host vuoto = manca.
bool parseArgs(const char* line, OutSink& o, Opts& p) {
  bool haveHost = false;
  for (int i = 1; i < 16; i++) {
    Word w = argAt(line, i);
    if (w.empty()) break;
    if (w.cut()) { o.println(tr("Comando troppo lungo")); return false; }
    if (w.c_str()[0] != '-' || w.length() < 2) {
      if (haveHost) { o.println(tr("ping: un solo host per volta")); return false; }
      p.host = w;
      haveHost = true;
      continue;
    }
    char opt = w.c_str()[1];
    if (!strchr("ciWst", opt)) { o.println(trf("ping: opzione non valida -- '%c'", opt)); cmdUsage(o, "ping"); return false; }
    Word next;
    const char* val = w.c_str() + 2;
    if (!*val) {
      next = argAt(line, ++i);
      val = next.c_str();
      if (next.empty()) { o.println(trf("ping: l'opzione -%c vuole un valore", opt)); return false; }
    }
    if (!setOption(opt, val, p, o)) return false;
  }
  if (!haveHost) { o.println(tr("ping: serve l'indirizzo di destinazione")); cmdUsage(o, "ping"); return false; }
  return true;
}

// Durata massima = pausa tra i pacchetti per tutti tranne l'ultimo, piu l'attesa dell'ultimo. Si controlla PRIMA di cominciare.
bool durationOk(const Opts& p, uint32_t sess, OutSink& o) {
  uint32_t limit = sess == SESS_SERIAL ? kMaxSerialMs : kMaxWebMs;
  uint32_t worst = (p.count - 1) * p.intervalMs + p.timeoutMs;
  if (worst <= limit) return true;
  o.println(trf("ping: durata massima %u s (%s): riduci -c, -i o -W", (unsigned)(limit / 1000), sess == SESS_SERIAL ? tr("seriale") : tr("pagina web e script")));
  return false;
}

// ---------- destinazione ----------

// Un nome valido: lettere, cifre, '-', '_' e '.', fino a 63 caratteri, non comincia con '.' o '-'.
bool nameValid(const char* h) {
  size_t n = strlen(h);
  if (n == 0 || n > 63 || h[0] == '.' || h[0] == '-') return false;
  for (size_t i = 0; i < n; i++) {
    char c = h[i];
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
    if (!ok) return false;
  }
  return true;
}

bool endsLocal(const char* h) {
  size_t n = strlen(h);
  if (n < 7) return false;
  const char* t = h + n - 6;
  return (t[0] == '.') && (t[1] | 32) == 'l' && (t[2] | 32) == 'o' && (t[3] | 32) == 'c' && (t[4] | 32) == 'a' && (t[5] | 32) == 'l';
}

// La scheda ha una rete su cui spedire? Client collegato: qualunque indirizzo. Solo hotspot: solo la sua rete (192.168.4.x).
bool routable(uint32_t ip) {
  NetState s = netState();
  if (s == NET_CLIENT_OK) return true;
  return s == NET_AP && netOnAp(ip);
}

// Indirizzi che un ping non deve mai raggiungere: 0.x.x.x, loopback (127/8), multicast e riservati (224.0.0.0 e oltre,
// compreso 255.255.255.255) e il broadcast della rete locale (calcolato con la maschera; sull'hotspot x.x.x.255).
// Scelta: NON si rifiutano tutti gli indirizzi che finiscono per .255, perche in una rete piu larga di /24 sono indirizzi normali.
bool addressAllowed(uint32_t ip) {
  uint32_t a = ip >> 24;
  if (a == 0 || a == 127 || a >= 224) return false;
  uint32_t mask = drvWifiMask();
  uint32_t local = drvWifiLocalIp();
  if (mask && local && ip == ((local & mask) | ~mask)) return false;
  if (netOnAp(ip) && (ip & 0xFF) == 0xFF) return false;
  return true;
}

// Da testo a indirizzo: numerico diretto; ".local" con mDNS; altri nomi con il DNS. false = "Name or service not known".
bool resolveTarget(const char* host, uint32_t& ip) {
  if (ipParse(host, ip)) return true;
  if (!nameValid(host)) return false;
  if (endsLocal(host)) return drvMdnsResolve(host, kNameTimeoutMs, ip);       // se mDNS e spento non trova niente
  return drvDnsResolve(host, kNameTimeoutMs, ip);
}

// ---------- uscita ----------

struct Out {
  OutSink* o;
  char     ip[16];       // l'indirizzo di destinazione, scritto
};

// Un tempo in microsecondi con un decimale: 1250 -> "1.3". I millisecondi misurati da ESP-IDF sono interi (vedi vos_drv_ping.h).
void oneDecimal(char* out, size_t cap, uint32_t us) {
  uint32_t tenths = (us + 50) / 100;
  snprintf(out, cap, "%lu.%lu", (unsigned long)(tenths / 10), (unsigned long)(tenths % 10));
}

// Una riga per pacchetto, appena concluso: la risposta, oppure "no answer yet" (come ping -O di iputils).
void onSample(const DrvPingSample& s, void* user) {
  Out& w = *(Out*)user;
  if (!s.ok) { w.o->printf("no answer yet for icmp_seq=%u\r\n", (unsigned)s.seq); return; }
  char tm[12];
  if (s.us < 1000) snprintf(tm, sizeof tm, "<1");
  else snprintf(tm, sizeof tm, "%lu", (unsigned long)(s.us / 1000));
  w.o->printf("%u bytes from %s: icmp_seq=%u ttl=%u time=%s ms\r\n", (unsigned)s.size, w.ip, (unsigned)s.seq, (unsigned)s.ttl, tm);
}

// Sulla seriale Ctrl+C (codice 3) ferma il ping; gli altri tasti si scartano finche dura. Dalla pagina web non c'e niente da leggere.
bool serialCancel(void*) {
  bool stop = false;
  while (drvConAvailable() > 0)
    if (drvConRead() == 3) stop = true;
  return stop;
}

// Le statistiche finali, come iputils.
void printStats(OutSink& o, const char* host, const DrvPingResult& r) {
  o.printf("\r\n--- %s ping statistics ---\r\n", host);
  unsigned loss = r.sent ? (unsigned)(((uint64_t)(r.sent - r.received) * 100) / r.sent) : 0;
  o.printf("%u packets transmitted, %u received, %u%% packet loss, time %ums\r\n", (unsigned)r.sent, (unsigned)r.received, loss, (unsigned)r.totalMs);
  if (!r.received) return;
  char mn[16], av[16], mx[16], md[16];
  oneDecimal(mn, sizeof mn, r.minUs);
  oneDecimal(av, sizeof av, r.avgUs);
  oneDecimal(mx, sizeof mx, r.maxUs);
  oneDecimal(md, sizeof md, r.mdevUs);
  o.printf("rtt min/avg/max/mdev = %s/%s/%s/%s ms\r\n", mn, av, mx, md);
}

void printRunError(OutSink& o, DrvPingErr e) {
  switch (e) {
    case DP_BUSY:    o.println(tr("ping: occupato, un altro ping e in corso")); break;
    case DP_BAD_ARG: o.println(tr("ping: valori non validi")); break;
    case DP_NO_MEM:  o.println(tr("Memoria insufficiente")); break;
    default:         o.println(tr("ping: non riesco ad avviare la sessione")); break;
  }
}

// Il ping vero: controlli, destinazione, intestazione, sessione, statistiche.
void runPing(const char* line, OutSink& o, uint32_t sess) {
  Opts p;
  if (!parseArgs(line, o, p) || !durationOk(p, sess, o)) return;

  // Rete: client collegato = si puo andare ovunque. Solo hotspot = solo indirizzi numerici della sua rete (i nomi non si risolvono).
  NetState ns = netState();
  uint32_t ip = 0;
  if (ns != NET_CLIENT_OK && !(ns == NET_AP && ipParse(p.host.c_str(), ip))) { o.println(tr("ping: connect: rete non raggiungibile")); return; }
  if (!resolveTarget(p.host.c_str(), ip)) { o.println(trf("ping: %s: Nome o servizio sconosciuto", p.host.c_str())); return; }
  if (!addressAllowed(ip)) { o.println(tr("ping: indirizzo non ammesso (broadcast, multicast, riservato o locale)")); return; }
  if (!routable(ip)) { o.println(tr("ping: connect: rete non raggiungibile")); return; }

  Out w;
  w.o = &o;
  ipTo(ip, w.ip, sizeof w.ip);
  vlog("ping %s", w.ip);                                     // solo nel registro locale della scheda
  o.printf("PING %s (%s) %u(%u) bytes of data.\r\n", p.host.c_str(), w.ip, (unsigned)p.size, (unsigned)(p.size + kIcmpHeader + kIpHeader));

  DrvPingReq req = {ip, p.count, p.intervalMs, p.timeoutMs, p.size, p.ttl};
  DrvPingResult r;
  DrvPingErr e = drvPingRun(req, onSample, sess == SESS_SERIAL ? serialCancel : nullptr, &w, r);
  if (e != DP_OK) { printRunError(o, e); return; }
  if (r.cancelled) o.print("^C");
  printStats(o, p.host.c_str(), r);
  if (r.timedOut) o.println(tr("ping: scaduto il tempo di sicurezza, sessione fermata"));
}

}  // namespace

// ping [-c n] [-i secondi] [-W secondi] [-s byte] [-t ttl] host   (chiamato da shCmdBasic: serve la sessione per il tetto della durata)
void shPing(const char* line, OutSink& o, uint32_t sess) { runPing(line, o, sess); }
