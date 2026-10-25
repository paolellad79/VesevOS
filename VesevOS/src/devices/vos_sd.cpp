// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_sd.cpp (vedi vos_sd.h)
// Montaggio, smontaggio, stato e prova della microSD. Le azioni sui file stanno in vos_sd_act.cpp.
// Niente Arduino: solo C, buffer con lunghezza e il driver vos_drv_sd.
#include "vos_sd.h"
#if VOS_WITH_SD
#include <stdio.h>
#include <string.h>
#include "vos_sd_buf.h"
#include "vos_io.h"
#include "../core/vos_i18n.h"
#include "../core/vos_log.h"
#include "../drivers/vos_drv_sd.h"

static const int SD_TEST_BYTES = 4096;     // dimensione del file della prova
static const int ST_MOUNTED = SD_ST_MOUNTED;
static const int ST_NOT_MOUNTED = SD_ST_NOT_MOUNTED;
static const int ST_NO_CARD = SD_ST_NO_CARD;           // tolta o assente al montaggio, o nessuna risposta dal modulo
static const int ST_BAD_FORMAT = SD_ST_BAD_FORMAT;     // exFAT o non FAT

static char g_lastError[SD_ERR_MAX] = "";  // motivo dell'ultimo montaggio fallito (vuoto = nessuno)
static uint8_t g_state = 0;                // stato noto dell'ultimo montaggio: 0 normale, 3 o 4 come sopra
static volatile uint8_t g_busy = 0;        // 1 mentre si monta o si smonta (i task web e shell sono diversi)
static volatile int g_ops = 0;             // azioni di sdAct in corso adesso (web e shell sono task diversi)

// Fa comparire queste frasi nel catalogo delle traduzioni.
[[maybe_unused]] static void translationKeys() {
  tr("Risorse");
  tr("microSD");
}

bool sdUp() {
  return drvSdUp();
}

// La microSD e una periferica di io.cfg (classe "sd"): i pin li occupa vos_io.
bool sdConfigured() {
  int pins[5];
  return ioSdPins(pins);
}

// ---- Montaggio ----

static const char* failText(uint8_t fail) {
  if (fail == DSF_EXFAT) {
    return tr("Questa scheda e exFAT: serve FAT32. Formattala in FAT32 sul computer (VesevOS non formatta mai da solo)");
  }
  if (fail == DSF_BAD_FS) {
    return tr("La scheda risponde ma non e FAT/FAT32: serve FAT32. Formattala sul computer (VesevOS non formatta mai da solo)");
  }

  return tr("Nessuna risposta dal modulo: controlla il cablaggio, i 3,3 V e che la scheda sia inserita");
}

static const char* failLogName(uint8_t fail) {
  if (fail == DSF_EXFAT) {
    return "exFAT";
  }
  if (fail == DSF_BAD_FS) {
    return "formato non FAT";
  }
  if (fail == DSF_NO_RESPONSE) {
    return "dati non validi dal modulo";
  }

  return "nessuna scheda";
}

static void rememberMountFailure(uint8_t fail) {
  bool formatProblem = fail == DSF_EXFAT || fail == DSF_BAD_FS;

  g_state = formatProblem ? ST_BAD_FORMAT : ST_NO_CARD;
  snprintf(g_lastError, sizeof g_lastError, "%s", failText(fail));
  vlog("SD: montaggio fallito (%s)", failLogName(fail));
}

static bool mountWithPins() {
  int pins[5];

  if (!ioSdPins(pins)) {
    return false;
  }

  uint32_t hz = (uint32_t)pins[4] * 1000UL;
  return drvSdBegin(pins[0], pins[1], pins[2], pins[3], hz);
}

bool sdMount(char* err, size_t en) {
  if (__sync_lock_test_and_set(&g_busy, 1)) {
    sdSetErr(err, en, tr("Operazione gia in corso"));
    return false;
  }

  bool mounted = false;

  if (drvSdUp()) {
    mounted = true;
  } else if (!sdConfigured()) {
    sdSetErr(err, en, tr("Nessuna microSD configurata: aggiungila da Periferiche"));
  } else if (!mountWithPins()) {
    rememberMountFailure(drvSdLastFail());
    sdSetErr(err, en, g_lastError);
  } else {
    g_lastError[0] = 0;
    g_state = 0;
    mounted = true;
    vlog("SD: montata, %u MB", (unsigned)(drvSdCardBytes() / (1024ULL * 1024ULL)));
  }

  __sync_lock_release(&g_busy);
  return mounted;
}

bool sdUnmount(char* err, size_t en) {
  if (__sync_lock_test_and_set(&g_busy, 1)) {
    sdSetErr(err, en, tr("Operazione gia in corso"));
    return false;
  }

  if (drvSdUp()) {
    vlog("SD: smontata");
  }

  drvSdEnd();
  g_state = 0;
  g_lastError[0] = 0;

  __sync_lock_release(&g_busy);
  return true;
}

// ---- Stato ----

static const char* typeName(uint8_t type) {
  switch (type) {
    case DSD_MMC:  return "MMC";
    case DSD_SD:   return "SDSC";
    case DSD_SDHC: return "SDHC/SDXC";
    case DSD_NONE: return "-";
    default:       return "?";
  }
}

static const char* fsName(uint8_t fs) {
  switch (fs) {
    case DFS_FAT12: return "FAT12";
    case DFS_FAT16: return "FAT16";
    case DFS_FAT32: return "FAT32";
    case DFS_EXFAT: return "exFAT";
    default:        return "?";
  }
}

static void formatMegabytes(char* out, size_t n, uint64_t bytes) {
  snprintf(out, n, "%lu MB", (unsigned long)(bytes / (1024ULL * 1024ULL)));
}

// Stato per pagina e Home: 1 montata, 2 non montata, 3 scheda non presente, 4 formato non supportato.
// Se e montata si legge un settore: se la scheda e stata tolta si smonta da sola
// (i pin tornano liberi) e lo stato diventa 3.
static int currentState() {
  if (drvSdUp() && !drvSdAlive()) {
    g_state = ST_NO_CARD;
    snprintf(g_lastError, sizeof g_lastError, "%s", tr("La scheda e stata tolta: inseriscila e premi Monta"));
    vlog("SD: scheda tolta mentre era montata: smontata");
  }

  if (drvSdUp()) {
    return ST_MOUNTED;
  }
  if (g_state == ST_NO_CARD) {
    return ST_NO_CARD;
  }
  if (g_state == ST_BAD_FORMAT) {
    return ST_BAD_FORMAT;
  }

  return ST_NOT_MOUNTED;
}

static uint64_t freeBytes(uint64_t total, uint64_t used) {
  return total > used ? total - used : 0;
}

// Le righe di dettaglio della scheda montata.
static void mountedRows(Buf& j, bool& first, uint8_t fs, uint64_t card, uint64_t total, uint64_t used) {
  char text[96];

  sdRow(j, first, tr("Tipo"), typeName(drvSdType()));
  sdRow(j, first, tr("Formato"), fsName(fs));

  formatMegabytes(text, sizeof text, card);
  sdRow(j, first, tr("Capacita"), text);

  formatMegabytes(text, sizeof text, total);
  sdRow(j, first, tr("Volume FAT"), text);

  formatMegabytes(text, sizeof text, used);
  sdRow(j, first, tr("Usato"), text);

  formatMegabytes(text, sizeof text, freeBytes(total, used));
  sdRow(j, first, tr("Libero"), text);
}

// Le righe con i pin e la frequenza scelti nella periferica.
static void pinRows(Buf& j, bool& first, const int* pins) {
  char text[96];

  snprintf(text, sizeof text, "CS %d, MOSI %d, SCK %d, MISO %d", pins[0], pins[1], pins[2], pins[3]);
  sdRow(j, first, tr("Pin"), text);

  snprintf(text, sizeof text, "%d kHz", pins[4]);
  sdRow(j, first, tr("Frequenza SPI"), text);
}

void sdStatusJson(char* out, size_t on) {
  Buf j(out, on);
  int state = currentState();
  bool mounted = state == ST_MOUNTED;
  bool first = true;

  uint64_t card = drvSdCardBytes();
  uint64_t total = drvSdTotalBytes();
  uint64_t used = drvSdUsedBytes();
  uint8_t fs = mounted ? drvSdFsType() : (uint8_t)DFS_UNKNOWN;

  j.put("{\"rows\":[");
  sdRow(j, first, tr("Montata"), mounted ? tr("si") : tr("no"));

  if (mounted) {
    mountedRows(j, first, fs, card, total, used);
  } else if (g_lastError[0]) {
    sdRow(j, first, tr("Ultimo errore"), g_lastError);
  }

  int pins[5];
  bool configured = ioSdPins(pins);

  if (configured) {
    pinRows(j, first, pins);
  }

  // Capacita in KB.
  j.fmt("],\"cfg\":%d,\"s\":%d,\"up\":%d,\"type\":\"%s\",\"fs\":\"%s\",\"cap\":%lu,\"total\":%lu,\"used\":%lu}",
        configured ? 1 : 0, configured ? state : 0, mounted ? 1 : 0, typeName(drvSdType()), mounted ? fsName(fs) : "",
        (unsigned long)(card / 1024), (unsigned long)(total / 1024), (unsigned long)(used / 1024));
}

// Per il widget della Home: testo vuoto se la microSD non e configurata.
void sdHomeJson(char* out, size_t on) {
  if (!on) {
    return;
  }

  out[0] = 0;

  int pins[5];

  if (!ioSdPins(pins)) {
    return;
  }

  int state = currentState();
  uint64_t card = drvSdCardBytes();
  uint64_t total = drvSdTotalBytes();
  uint64_t used = drvSdUsedBytes();
  const char* fs = state == ST_MOUNTED ? fsName(drvSdFsType()) : "";

  snprintf(out, on, "{\"s\":%d,\"fs\":\"%s\",\"cap\":%lu,\"free\":%lu,\"used\":%lu,\"khz\":%d}",
           state, fs, (unsigned long)(card / 1024), (unsigned long)(freeBytes(total, used) / 1024),
           (unsigned long)(used / 1024), pins[4]);
}

// Spazio del volume per il comando df: solo se la microSD e montata adesso (altrimenti false e niente riga).
bool sdSpaceKb(uint32_t& totalKb, uint32_t& usedKb) {
  if (currentState() != ST_MOUNTED) {
    return false;
  }

  totalKb = (uint32_t)(drvSdTotalBytes() / 1024);
  usedKb = (uint32_t)(drvSdUsedBytes() / 1024);
  return true;
}

bool sdVolume(SdVolume& v) {
  int pins[5];
  memset(&v, 0, sizeof v);

  if (!ioSdPins(pins)) {
    return false;
  }

  v.state = (uint8_t)currentState();
  v.khz = (uint32_t)pins[4];

  if (v.state == ST_MOUNTED) {
    snprintf(v.fs, sizeof v.fs, "%s", fsName(drvSdFsType()));
    v.totalKb = (uint32_t)(drvSdTotalBytes() / 1024);
    v.usedKb = (uint32_t)(drvSdUsedBytes() / 1024);
  }

  return true;
}

bool sdInUse() { return g_busy || g_ops > 0; }
void sdOpEnter() { __sync_fetch_and_add(&g_ops, 1); }
void sdOpLeave() { __sync_fetch_and_sub(&g_ops, 1); }

void sdText(char* out, size_t on) {
  if (!on) {
    return;
  }

  if (!sdConfigured()) {
    snprintf(out, on, "%s", tr("microSD: non configurata (aggiungila da Periferiche)"));
    return;
  }

  if (currentState() != ST_MOUNTED) {
    const char* open = g_lastError[0] ? " (" : "";
    const char* close = g_lastError[0] ? ")" : "";
    snprintf(out, on, "%s%s%s%s", tr("microSD: non montata"), open, g_lastError, close);
    return;
  }

  char card[24];
  char used[24];
  formatMegabytes(card, sizeof card, drvSdCardBytes());
  formatMegabytes(used, sizeof used, drvSdUsedBytes());

  snprintf(out, on, tr("microSD: montata, %s, %s, %s, usato %s"), typeName(drvSdType()), fsName(drvSdFsType()), card, used);
}

// ---- Prova ----

bool sdProve(char* out, size_t on, char* err, size_t en) {
  uint32_t writeMs = 0;
  uint32_t readMs = 0;
  uint8_t result = drvSdProve(SD_TEST_BYTES, writeMs, readMs);

  if (result == DSP_NOT_UP) {
    sdSetErr(err, en, tr("La microSD non e montata"));
    return false;
  }
  if (result == DSP_NO_MEM) {
    sdSetErr(err, en, tr("Memoria insufficiente"));
    return false;
  }
  if (result == DSP_NO_WRITE) {
    sdSetErr(err, en, tr("Prova fallita: non riesco a scrivere (scheda protetta, piena o tolta?)"));
    return false;
  }
  if (result != DSP_OK) {
    sdSetErr(err, en, tr("Prova fallita: i dati letti non coincidono con quelli scritti"));
    return false;
  }

  snprintf(out, on, "{\"ok\":true,\"bytes\":%d,\"wms\":%lu,\"rms\":%lu}", SD_TEST_BYTES, (unsigned long)writeMs, (unsigned long)readMs);
  vlog("SD: prova scrittura/lettura riuscita (%d byte, %lu ms + %lu ms)", SD_TEST_BYTES, (unsigned long)writeMs, (unsigned long)readMs);
  return true;
}

#endif
