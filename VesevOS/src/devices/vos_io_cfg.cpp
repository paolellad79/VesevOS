// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_io_cfg.cpp
// Salvataggio sicuro di /io.cfg per vos_io (spostato qui da vos_io.cpp: file sotto le 500 righe). Nessun cambio di comportamento.
#include "vos_io.h"
#include "vos_io_int.h"
#include "../core/vos_common.h"
#include "../core/vos_i18n.h"
#include "../core/vos_log.h"
#include "../drivers/vos_drv_fs.h"
#include "../core/vos_audit.h"

static char g_junk[IO_JUNK_MAX + 1];
static size_t g_junkLen = 0;
static uint8_t g_pend = 0;                          // avvisi dell'avvio da mandare all'audit appena e pronto (ioTick): 1 copia usata, 2 illeggibile, 4 righe tenute, 8 nome cambiato
static bool g_saveBad = false;                      // l'ultimo salvataggio e fallito: l'allarme "iocfg" e acceso

// Scrittura sicura: file temporaneo controllato (byte scritti, close, dimensione), copia dell'ultima versione buona in .bak, poi lo scambio.
// Mai un io.cfg a 0 byte: lista vuota = file tolto. Se qualcosa fallisce il file di prima resta com'e. skip = periferica da non scrivere, extra = una in piu.
static size_t lineOf(char* out, size_t room, const IoDev& d) {
  int n;
  if (d.cls == IC_SD || d.cls == IC_PWM || d.cls == IC_CNT) n = snprintf(out, room, "%s %u %d %u %d %u %u %u\n", d.name, (unsigned)d.cls, (int)d.gpio, (unsigned)d.flags, (int)d.gpio2, (unsigned)d.addr, (unsigned)d.drv, (unsigned)d.khz);
  else if (d.cls == IC_LEDRGB) n = snprintf(out, room, "%s %u %d %u %u %u %u\n", d.name, (unsigned)d.cls, (int)d.gpio, (unsigned)(d.on ? IOF_ACCESO : 0), (unsigned)d.cPrev, (unsigned)d.addr, (unsigned)d.drv);   // led_rgb: <nome> 10 <gpio> <acceso 0/16> <colore> <luminosita> <ordine>
  else if (d.cls == IC_I2C) n = snprintf(out, room, "%s %u %d %u %d %u %u\n", d.name, (unsigned)d.cls, (int)d.gpio, (unsigned)d.flags, (int)d.gpio2, (unsigned)d.addr, (unsigned)d.drv);
  else n = snprintf(out, room, "%s %u %d %u\n", d.name, (unsigned)d.cls, (int)d.gpio, (unsigned)d.flags);
  return (n > 0 && (size_t)n < room) ? (size_t)n : 0;
}
static const char* writeCfg(const IoDev* list, int n, int skip, const IoDev* extra) {
  static char buf[IO_MAX * 48 + IO_JUNK_MAX + 8];       // sotto il blocco: nessuna corsa
  size_t len = 0;
  for (int i = 0; i <= n; i++) {
    const IoDev* d = (i < n) ? (i == skip ? nullptr : &list[i]) : extra;
    if (d) len += lineOf(buf + len, sizeof buf - len, *d);
  }
  memcpy(buf + len, g_junk, g_junkLen); len += g_junkLen; buf[len] = 0;
  if (!len) {                                           // niente da tenere: si toglie il file (prima la copia, cosi un .bak vecchio non torna mai)
    drvFsRemove(IO_BAK);
    if (drvFsExists(IO_FILE) && !drvFsRemove(IO_FILE)) return tr("Impossibile scrivere");
    return nullptr;
  }
  size_t used = drvFsUsed(), total = drvFsTotal();
  if (len + 8192 > (used < total ? total - used : 0)) return tr("Spazio esaurito");
  VosFile f = drvFsOpen(IO_TMP, "w");
  if (!f) return tr("Impossibile scrivere");
  size_t w = f.write((const uint8_t*)buf, len);
  f.close();
  bool good = (w == len);
  if (good) { VosFile c = drvFsOpen(IO_TMP, "r"); good = c && (size_t)c.size() == len; c.close(); }
  if (!good) { drvFsRemove(IO_TMP); return tr("Spazio esaurito"); }
  bool moved = false;
  if (drvFsExists(IO_FILE)) {
    VosFile m = drvFsOpen(IO_FILE, "r"); size_t ms = m ? (size_t)m.size() : 0; m.close();
    if (ms > 0) {
      drvFsRemove(IO_BAK);
      if (!drvFsRename(IO_FILE, IO_BAK)) { drvFsRemove(IO_TMP); return tr("Impossibile scrivere"); }
      moved = true;
    } else if (!drvFsRemove(IO_FILE)) { drvFsRemove(IO_TMP); return tr("Impossibile scrivere"); }
  }
  if (!drvFsRename(IO_TMP, IO_FILE)) {
    if (moved) drvFsRename(IO_BAK, IO_FILE);
    drvFsRemove(IO_TMP);
    return tr("Impossibile scrivere");
  }
  return nullptr;
}
const char* ioCfgSave(const IoDev* list, int n, int skip, const IoDev* extra) {
  const char* e = writeCfg(list, n, skip, extra);
  if (e) {
    vlog("ERRORE: IO: non riesco a salvare %s (%s): l'elenco non cambia", IO_FILE, e);
    auditEvent(AUD_YELLOW, "iocfg", tr("Impossibile salvare le periferiche (io.cfg): nessuna modifica fatta. Controlla lo spazio libero"));
    g_saveBad = true;
  } else if (g_saveBad) { auditClear("iocfg"); g_saveBad = false; }
  return e;
}
void ioCfgKeepRow(const char* row) {                  // riga valida ma non caricata (pin occupato): resta nel file
  size_t k = strlen(row);
  if (k > 100 || g_junkLen + k + 1 > IO_JUNK_MAX) { vlog("ERRORE: IO: riga non tenuta (troppo lunga o troppe righe): %s", row); return; }
  memcpy(g_junk + g_junkLen, row, k); g_junkLen += k; g_junk[g_junkLen++] = '\n'; g_junk[g_junkLen] = 0;
  g_pend |= 4;
}

void ioCfgReset() { g_junkLen = 0; g_junk[0] = 0; g_pend = 0; }
// Una riga di io.cfg: <nome> <classe> <gpio> <flags> [<gpio2> <addr> <drv> [<khz>]] (led_rgb: al posto di gpio2 c'e il colore). false = riga da saltare (come prima, senza messaggio).
bool ioCfgParseRiga(const char* l, char* nm, size_t nmCap, unsigned& c, int& g, unsigned& fl, int& g2, unsigned& ad, unsigned& dr, unsigned& kh) {
  const char* p = l;
  size_t k = 0;
  while (*p && *p != ' ' && *p != '\t') { if (k + 1 < nmCap) nm[k++] = *p; p++; }
  nm[k] = 0;
  long x[7];
  int got = 0;
  char* e;
  while (got < 7) { x[got] = strtol(p, &e, 10); if (e == p) break; p = e; got++; }
  if (!k || (got != 3 && got != 6 && got != 7) || x[0] < 0 || x[2] < 0) return false;
  c = (unsigned)x[0]; g = (int)x[1]; fl = (unsigned)x[2];
  if (got >= 6) { g2 = (int)x[3]; ad = (unsigned)x[4]; dr = (unsigned)x[5]; }
  if (got == 7) kh = (unsigned)x[6];
  return true;
}

void ioCfgPend(uint8_t bits) { g_pend |= bits; }
void ioCfgPendFlush() {
  if (!g_pend) return;
  uint8_t p = g_pend; g_pend = 0;
  if (p & 1) auditEvent(AUD_YELLOW, "iocfgbad", tr("io.cfg era vuoto o illeggibile: uso la copia di sicurezza (io.cfg.bak)"));
  if (p & 2) auditEvent(AUD_YELLOW, "iocfgbad", tr("io.cfg illeggibile e senza copia di sicurezza: nessuna periferica caricata"));
  if (p & 8) auditEvent(AUD_YELLOW, "iocfgren", tr("Una periferica a mano aveva il nome di un LED della scheda: e stata rinominata. Controlla le regole e i programmi che la usano"));
  if (p & 4) auditEvent(AUD_YELLOW, "iocfgrow", tr("Alcune periferiche di io.cfg non sono state caricate (pin occupato): restano nel file"));
}
