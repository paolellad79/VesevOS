// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_audit.cpp
#include "vos_audit.h"
#include "vos_eventbus.h"
#include "vos_config.h"
#include "vos_region.h"
#include "vos_i18n.h"
#include "vos_log.h"
#include "vos_heapbuf.h"
#include "vos_time.h"
#include "vos_hooks.h"
#include "../drivers/vos_drv_fs.h"
#include <ctype.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "../drivers/vos_drv_time.h"

#define MAX_AL 16
#define MAX_HIST 20
#define KEY_MAX 24                                  // chiave dell'allarme (le chiavi vere sono corte: "wd_" + nome del servizio e la piu lunga)
// Il testo dell'allarme e dello storico sta in un blocco di heap della lunghezza giusta (liberato quando l'allarme sparisce)
struct Alarm { char key[KEY_MAX]; char* text; uint8_t lv; bool ack; bool used; uint32_t since; };
static Alarm g_al[MAX_AL];
static char* g_hist[MAX_HIST]; static int g_histN = 0;
// Allarmi e storico sono toccati da piu task (pagina HTTP, HTTPS, ciclo principale): un blocco ricorsivo li protegge.
static SemaphoreHandle_t g_aumx = xSemaphoreCreateRecursiveMutex();
struct AudLock { AudLock() { if (g_aumx) xSemaphoreTakeRecursive(g_aumx, portMAX_DELAY); } ~AudLock() { if (g_aumx) xSemaphoreGiveRecursive(g_aumx); } };

static const char* txt(const char* p) { return p ? p : ""; }
static void setText(char*& dst, const char* src) { free(dst); dst = strdup(src); }       // se manca la memoria resta nullptr: si legge come vuoto
static bool keyIs(const Alarm& a, const char* key) { return strncmp(a.key, key, KEY_MAX - 1) == 0 && strlen(key) < KEY_MAX; }
// Aggiunge una riga in coda a un elenco di MAX_HIST righe; se e pieno toglie la piu vecchia. L'elenco diventa proprietario della riga.
static void ringPush(char** arr, int& n, char* line) {
  if (n < MAX_HIST) { arr[n++] = line; return; }
  free(arr[0]);
  memmove(arr, arr + 1, (MAX_HIST - 1) * sizeof(char*));
  arr[MAX_HIST - 1] = line;
}

// Lo storico resta anche dopo un riavvio: ogni riga va anche in /audit.log (file di sistema, nascosto alle API dei file).
// Il file si riscrive con le sole ultime righe quando supera HIST_FILE_MAX: poca scrittura sulla flash.
#define HIST_FILE "/audit.log"
#define HIST_FILE_MAX 6144
#define HIST_LINE_MAX 512                           // righe del file piu lunghe vengono tagliate
static bool g_histLoaded = false;
static void histLoad() {
  if (g_histLoaded || !drvFsTotal()) return;           // file system non ancora pronto: si riprova alla prossima riga
  g_histLoaded = true;
  VosFile f = drvFsOpen(HIST_FILE, "r");
  if (!f) return;
  char* line = (char*)malloc(HIST_LINE_MAX);
  if (!line) { f.close(); return; }
  char* old[MAX_HIST] = {}; int n = 0;
  while (f.available()) {
    size_t len = 0;
    for (int c; (c = f.read()) >= 0 && c != '\n'; ) if (len < HIST_LINE_MAX - 1) line[len++] = (char)c;
    while (len && isspace((unsigned char)line[len - 1])) len--;
    line[len] = 0;
    const char* start = line; while (*start && isspace((unsigned char)*start)) start++;
    if (*start) { char* copy = strdup(start); if (copy) ringPush(old, n, copy); }
  }
  f.close();
  free(line);
  // le righe vecchie vanno prima di quelle scritte in questo avvio prima del caricamento
  char* cur[MAX_HIST]; int cn = g_histN; memcpy(cur, g_hist, sizeof(char*) * cn);
  g_histN = 0;
  for (int i = 0; i < n; i++) ringPush(g_hist, g_histN, old[i]);
  for (int i = 0; i < cn; i++) ringPush(g_hist, g_histN, cur[i]);
}
static void histSave(const char* l) {
  if (!drvFsTotal()) return;
  VosFile f = drvFsOpen(HIST_FILE, "a");
  if (!f) return;
  size_t sz = f.size(); f.print(l); f.print('\n'); f.close();
  if (sz + strlen(l) + 1 <= HIST_FILE_MAX) return;
  f = drvFsOpen(HIST_FILE, "w");                         // troppo grande: restano le ultime righe
  if (!f) return;
  for (int i = 0; i < g_histN; i++) { f.print(g_hist[i]); f.print('\n'); }
  f.close();
}

static void hist(const char* line) {
  AudLock L;
  histLoad();
  char now[32]; timeNowFmt(now, sizeof now);
  size_t total = strlen(now) + 2 + strlen(line) + 1;
  char* l = (char*)malloc(total);
  if (!l) return;
  snprintf(l, total, "%s  %s", now, line);
  for (char* p = l; *p; p++) if (*p == '\n') *p = ' ';
  ringPush(g_hist, g_histN, l);
  histSave(l);
}

static void ledUpdate() {
  int red = 0, yel = 0;
  for (int i = 0; i < MAX_AL; i++) if (g_al[i].used && !g_al[i].ack) { if (g_al[i].lv == AUD_RED) red++; else if (g_al[i].lv == AUD_YELLOW) yel++; }
  hookLedAlarm(red ? 2 : (yel ? 1 : 0));
}

void auditEvent(AuditLevel lv, const char* key, const char* text) {
  AudLock L;
  if (lv == AUD_INFO) { vlog("AUDIT: %s", text); hist(text); return; }
  int slot = -1;
  for (int i = 0; i < MAX_AL; i++) {
    if (g_al[i].used && keyIs(g_al[i], key)) {
      if (strcmp(txt(g_al[i].text), text) != 0 || g_al[i].lv != lv) { setText(g_al[i].text, text); g_al[i].lv = lv; g_al[i].ack = false; }
      ledUpdate(); return;
    }
    if (!g_al[i].used && slot < 0) slot = i;
  }
  if (slot < 0) slot = MAX_AL - 1;
  Alarm& a = g_al[slot];
  a.used = true; snprintf(a.key, sizeof(a.key), "%s", key); setText(a.text, text); a.lv = lv; a.ack = false; a.since = drvMillis();
  vlog("AUDIT: %s %s", lv == AUD_RED ? "ROSSO" : "GIALLO", text);
  EventBus::publish("audit.new", key, (int)lv);      // nuovo allarme (1 giallo, 2 rosso); il testo resta nell'audit
  size_t hn = strlen(text) + 10;
  char* h = (char*)malloc(hn);
  if (h) { snprintf(h, hn, "%s%s", lv == AUD_RED ? "[ROSSO] " : "[GIALLO] ", text); hist(h); free(h); }
  ledUpdate();
}

void auditClear(const char* key) {
  AudLock L;
  for (int i = 0; i < MAX_AL; i++) if (g_al[i].used && keyIs(g_al[i], key)) {
    g_al[i].used = false; g_al[i].key[0] = 0; free(g_al[i].text); g_al[i].text = nullptr; EventBus::publish("audit.clear", key, 0);
  }
  ledUpdate();
}

static void cond(bool on, AuditLevel lv, const char* key, const char* text) {
  if (on) auditEvent(lv, key, text); else auditClear(key);
}

static bool hasAdmin() {
  for (int i = 0; i < VOS_MAX_USERS; i++) if (cfg.users[i].name.length() && cfg.users[i].on && cfg.users[i].role == ROLE_ADMIN && cfg.users[i].hash.length() == 64) return true;
  return false;
}

// Valori vietati: si correggono sempre (anche se arrivano da un file ripristinato o modificato a mano)
void auditFix() {
  AudLock L;
  if (cfgApPassWeak(cfg.apPass.c_str())) {
    cfg.apPass = cfgNewApPass().c_str();
    auditEvent(AUD_RED, "appass", tr("Password dell'hotspot debole o uguale per tutti: sostituita con una nuova (vedi seriale o Rete > Punto di accesso)"));
  }
  if (cfg.country.length() && !regionValid(cfg.country.c_str())) {
    auditEvent(AUD_RED, "country", trf("Paese '%s' sconosciuto: tolto, valgono le regole prudenti", cfg.country.c_str()).c_str());
    cfg.country = "";
  }
  if (cfg.txDbm > regionMaxDbm()) {
    auditEvent(AUD_RED, "txpower", trf("Potenza %d dBm oltre il limite del paese: riportata a %d dBm", (int)cfg.txDbm, regionMaxDbm()).c_str());
    cfg.txDbm = 0;
  }
  if (!regionChannelOk(cfg.meshCh)) {
    auditEvent(AUD_RED, "meshch", trf("Canale %d non ammesso nel paese: riportato a 1", (int)cfg.meshCh).c_str());
    cfg.meshCh = 1;
  }
  if (cfg.antGain < 0 || cfg.antGain > 15) cfg.antGain = 0;
  if (cfg.fwMode > 3) cfg.fwMode = 0;
}

void auditRefresh() {
  AudLock L;
  cond(!cfgLastSaveOk(), AUD_RED, "saveerr", tr("Salvataggio della configurazione fallito: le modifiche si perdono al riavvio (memoria piena?)"));
  cond(!cfg.setupDone, AUD_YELLOW, "setupnd", tr("Guida di configurazione non finita: hotspot e HTTP restano forzati accesi a ogni avvio"));
  cond(cfg.setupDone && !hasAdmin(), AUD_RED, "noadmin", tr("Nessun amministratore attivo: tieni premuto BOOT 8 s per crearlo di nuovo"));
  cond(!cfg.country.length(), AUD_YELLOW, "nocountry", tr("Paese non scelto: radio con regole prudenti (canali 1-11). Sceglilo in Sistema > Localizzazione"));
  cond(!cfg.serialAuth, AUD_YELLOW, "serial", tr("Seriale senza password: chi collega il cavo USB entra senza password"));
  cond(cfg.mqttPass.length() && !cfg.mqttTls, AUD_YELLOW, "mqtttls", tr("MQTT con password ma senza cifratura (mqtts): la password viaggia in chiaro"));
  cond(!cfg.https, AUD_YELLOW, "https", tr("HTTPS spento: pagina e password viaggiano senza cifratura sulla rete di casa"));
  cond(!cfg.wdTask && !cfg.wdNet && !cfg.wdRam, AUD_YELLOW, "wd", tr("Watchdog spento: se un servizio si blocca la scheda non si riprende da sola"));
  cond(cfg.fwMode == 2 && cfg.fwN == 0, AUD_YELLOW, "fwempty", tr("Filtro IP 'lista consentita' senza voci: entra solo chi e collegato all'hotspot"));
  cond(cfg.antExt && cfg.antGain > 0, AUD_YELLOW, "antenna", trf("Antenna esterna %d dBi: potenza ridotta a %d dBm. Usa solo antenne adatte (responsabilita di chi la monta)", (int)cfg.antGain, regionMaxDbm()).c_str());
}

// Registro delle modifiche: confronta il testo di prima e di dopo (senza password) riga per riga
static bool hasText(const char* hay, const char* needle, size_t n) {       // needle (di n caratteri, non chiuso con 0) compare in hay?
  for (const char* p = hay; (p = strchr(p, needle[0])) != nullptr; p++) if (strncmp(p, needle, n) == 0) return true;
  return false;
}

// Nome della sezione ("config 'nome'") che precede la riga a pos: lo scrive in out (vuoto se non c'e)
static void sectionOf(const char* txt, size_t pos, char* out, size_t cap) {
  out[0] = 0;
  const char* p = txt + pos;
  while (p > txt && strncmp(p, "config ", 7) != 0) p--;
  if (strncmp(p, "config ", 7) != 0) return;
  const char* q1 = strchr(p, '\'');
  const char* q2 = q1 ? strchr(q1 + 1, '\'') : nullptr;
  if (q1 && q2 > q1) snprintf(out, cap, "%.*s", (int)(q2 - q1 - 1), q1 + 1);
}

void auditDiff(const char* o, const char* n, const char* origin) {
  AudLock L;
  int shown = 0;
  size_t nl = strlen(n);
  for (size_t i = 0; i < nl; ) {
    const char* e = strchr(n + i, '\n'); if (!e) e = n + nl;
    size_t ln = (size_t)(e - (n + i));
    const char* line = n + i;
    if (ln && strncmp(line, "\toption ", 8) == 0 && !hasText(o, line, ln)) {
      char sec[64], k[64], v[160];
      sectionOf(n, i, sec, sizeof sec);
      const char* rest = line + 8; size_t rl = ln - 8;
      const char* sp = (const char*)memchr(rest, ' ', rl);
      if (sp && sp > rest) {
        snprintf(k, sizeof k, "%.*s", (int)(sp - rest), rest);
        snprintf(v, sizeof v, "%.*s", (int)(rest + rl - sp - 1), sp + 1);
      } else {
        snprintf(k, sizeof k, "%.*s", (int)rl, rest);
        v[0] = 0;
      }
      if (shown < 6) auditEvent(AUD_INFO, "", trf("%s.%s = %s (%s)", sec, k, v, origin).c_str());
      shown++;
    }
    i += ln + 1;
  }
  if (shown > 6) auditEvent(AUD_INFO, "", trf("... e altre %d modifiche (%s)", shown - 6, origin).c_str());
}

void auditTick() {
  static uint32_t last = 0;
  if (last && drvMillis() - last < 60000UL) return;
  last = drvMillis(); if (!last) last = 1;
  if (cfgFileChanged()) {
    auditEvent(AUD_YELLOW, "tamper", tr("Configurazione modificata fuori dal pannello: controllata e corretta"));
    cfgSetOrigin("file modificato"); cfgSave();   // il file torna uguale alla configurazione in uso (controllata)
  }
}

void auditInit() {
  if (cfgFileChanged()) {
    auditEvent(AUD_YELLOW, "tamper", tr("Configurazione modificata fuori dal pannello: controllata e corretta"));
    cfgSetOrigin("file modificato"); cfgSave();
  }
  auditRefresh();
}

int auditCount(AuditLevel lv) {
  AudLock L;
  int n = 0;
  for (int i = 0; i < MAX_AL; i++) if (g_al[i].used && !g_al[i].ack && g_al[i].lv == lv) n++;
  return n;
}

void auditJson(HeapBuf& j) {
  AudLock L;
  histLoad();
  j.add("{\"alarms\":[");
  bool first = true;
  for (int i = 0; i < MAX_AL; i++) {
    if (!g_al[i].used) continue;
    if (!first) j.add(',');
    first = false;
    j.addf("{\"n\":%d,\"lv\":%u,\"ack\":%s,\"key\":\"", i + 1, (unsigned)g_al[i].lv, g_al[i].ack ? "true" : "false");
    j.addJson(g_al[i].key, strlen(g_al[i].key));
    j.add("\",\"text\":\"");
    j.addJson(txt(g_al[i].text), strlen(txt(g_al[i].text)));
    j.add("\"}");
  }
  j.add("],\"history\":[");
  for (int i = g_histN - 1; i >= 0; i--) { j.add('"'); j.addJson(g_hist[i], strlen(g_hist[i])); j.add('"'); if (i) j.add(','); }
  j.add("]}");
}

void auditText(HeapBuf& t) {
  AudLock L;
  int n = 0;
  for (int i = 0; i < MAX_AL; i++) {
    if (!g_al[i].used) continue;
    t.addf("%d  %s%s%s\n", i + 1, g_al[i].lv == AUD_RED ? tr("ROSSO ") : tr("GIALLO"), g_al[i].ack ? " (visto)  " : "  ", txt(g_al[i].text)); n++;
  }
  if (!n) t.addf("%s\n", tr("Nessun allarme: configurazione in regola"));
}

bool auditAck(int n) {
  AudLock L;
  bool any = false;
  for (int i = 0; i < MAX_AL; i++) {
    if (!g_al[i].used || (n && i != n - 1)) continue;
    g_al[i].ack = true; any = true;
  }
  ledUpdate();
  return any;
}
