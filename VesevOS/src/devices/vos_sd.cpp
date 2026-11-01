// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_sd.cpp (vedi vos_sd.h)
// Montaggio, smontaggio, stato e prova della microSD. Le azioni sui file stanno in vos_sd_io.cpp, il controllo dei percorsi in vos_sd_path.cpp.
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
static const int ST_BAD_FORMAT = SD_ST_BAD_FORMAT;     // formato non supportato o non FAT

static char g_lastError[SD_ERR_MAX] = "";  // motivo dell'ultimo montaggio fallito (vuoto = nessuno)
static uint8_t g_state = 0;                // stato noto dell'ultimo montaggio: 0 normale, 3 o 4 come sopra
static volatile uint8_t g_busy = 0;        // 1 mentre si monta o si smonta (i task web e shell sono diversi)
static volatile int g_ops = 0;             // operazioni sui file in corso adesso (web e shell sono task diversi)
static volatile bool g_ejected = false;    // espulsa a mano: il sondaggio non la rimonta fino a "monta" o al riavvio (NON si salva: al riavvio si rimonta sempre)
static bool g_autoTry = false;             // il montaggio in corso e un tentativo del sondaggio (vos_sd_auto.cpp)
static bool g_autoLogged = false;          // il tentativo fallito e gia nel registro: non si ripete ogni 2-3 secondi

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
    return tr("Formato non supportato: serve FAT32 (o FAT16 per schede piccole). Puoi formattarla dal pannello della microSD");
  }
  if (fail == DSF_BAD_FS) {
    return tr("La scheda risponde ma non e FAT/FAT32: serve FAT32. Puoi formattarla dal pannello della microSD");
  }

  return tr("Nessuna risposta dal modulo: controlla il cablaggio, i 3,3 V e che la scheda sia inserita");
}

static const char* failLogName(uint8_t fail) {
  if (fail == DSF_EXFAT) {
    return "formato non supportato";
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
  if (!g_autoTry || !g_autoLogged) {
    vlog("SD: montaggio fallito (%s)", failLogName(fail));
    g_autoLogged = g_autoTry;
  }
}

static bool mountWithPins() {
  int pins[5];

  if (!ioSdPins(pins)) {
    return false;
  }

  uint32_t hz = (uint32_t)pins[4] * 1000UL;
  return drvSdBegin(pins[0], pins[1], pins[2], pins[3], hz);
}

static bool mountCore(char* err, size_t en) {
  if (sdFormatting() || __sync_lock_test_and_set(&g_busy, 1)) {
    sdSetErr(err, en, sdBusyText());
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
    g_autoLogged = false;
    mounted = true;
    vlog("SD: montata, %u MB", (unsigned)(drvSdCardBytes() / (1024ULL * 1024ULL)));
  }

  __sync_lock_release(&g_busy);
  return mounted;
}

static int currentState();

// Montaggio chiesto da una persona (pagina, shell, programma): toglie anche il blocco dell'espulsione a mano.
bool sdMount(char* err, size_t en) {
  g_ejected = false;
  return mountCore(err, en);
}

// Un passo del sondaggio (lo chiama vos_sd_auto.cpp ogni 2-3 secondi, da un task a priorita bassa).
// - montata: si guarda se la scheda risponde (se e stata tolta, drvSdAlive smonta da solo e lo stato diventa "non rilevata");
// - non montata: si prova a montare. Non si prova se l'ha espulsa una persona, se il formato non e supportato (finche non si preme Monta o
//   si riavvia) o se c'e un'operazione in corso. Il bus lo tiene il driver con il suo blocco: se e occupato, il sondaggio salta il giro.
void sdAutoStep() {
  if (!sdConfigured() || g_ejected || g_busy || g_ops > 0 || sdFormatting()) {
    return;
  }
  if (drvSdUp()) {
    (void)currentState();
    return;
  }
  if (g_state == ST_BAD_FORMAT) {
    return;
  }

  char err[SD_ERR_MAX];

  g_autoTry = true;
  bool ok = mountCore(err, sizeof err);
  g_autoTry = false;
  if (ok) {
    vlog("SD: montata da sola appena rilevata");
  }
}

bool sdUnmount(char* err, size_t en) {
  if (sdFormatting() || __sync_lock_test_and_set(&g_busy, 1)) {
    sdSetErr(err, en, sdBusyText());
    return false;
  }

  if (drvSdUp()) {
    vlog("SD: smontata");
  }

  drvSdEnd();
  g_ejected = true;
  g_state = 0;
  g_lastError[0] = 0;

  __sync_lock_release(&g_busy);
  return true;
}

// Formattazione FAT32, voluta e confermata da un Admin (la conferma la chiedono shell, pagina e primitive); gira nel task di vos_sd_fmt.cpp. Cancella TUTTO. Mai la memoria interna.
// Funziona anche con una scheda che non si monta (exFAT, vuota). Alla fine la scheda e montata e il rimontaggio automatico torna libero.
bool sdFormat(char* err, size_t en) {
  int pins[5];

  if (!sdConfigured() || !ioSdPins(pins)) {
    sdSetErr(err, en, tr("Nessuna microSD configurata: aggiungila da Periferiche"));
    return false;
  }
  if (g_ops > 0) {
    sdSetErr(err, en, tr("La microSD e in uso: riprova tra un momento"));
    return false;
  }
  if (__sync_lock_test_and_set(&g_busy, 1)) {
    sdSetErr(err, en, sdBusyText());
    return false;
  }

  vlog("SD: formattazione FAT32 avviata");
  drvSdEnd();
  uint8_t r = drvSdFormat(pins[0], pins[1], pins[2], pins[3], (uint32_t)pins[4] * 1000UL);
  g_ejected = false;
  g_state = 0;
  g_lastError[0] = 0;
  __sync_lock_release(&g_busy);

  if (r == DSFM_OK) {
    vlog("SD: formattata FAT32, %u MB", (unsigned)(drvSdCardBytes() / (1024ULL * 1024ULL)));
    return true;
  }

  sdSetErr(err, en, r == DSFM_NO_CARD ? tr("Scheda non presente o non risponde") : tr("Formattazione non riuscita"));
  vlog("SD: formattazione non riuscita (%u)", (unsigned)r);
  return false;
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
    case DFS_EXFAT: return "non supportato";
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

// Spazio del volume per il comando df: solo se la microSD e montata adesso (altrimenti false e niente riga).
bool sdSpaceKb(uint32_t& totalKb, uint32_t& usedKb) {
  if (currentState() != ST_MOUNTED) {
    return false;
  }

  totalKb = (uint32_t)(drvSdTotalBytes() / 1024);
  usedKb = (uint32_t)(drvSdUsedBytes() / 1024);
  return true;
}

int sdStateNow() { return sdConfigured() ? currentState() : 0; }

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
    sdSetErr(err, en, sdFormatting() ? sdBusyText() : tr("La microSD non e montata"));
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
