// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_sd_act.cpp (vedi vos_sd.h)
// Azioni sui file della microSD (elenco, lettura, scrittura, cancellazione, cartelle) e controllo dei percorsi.
// Montaggio e stato stanno in vos_sd.cpp. Niente Arduino: solo C, buffer con lunghezza e il driver vos_drv_sd.
#include "vos_sd.h"
#if VOS_WITH_SD
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "vos_sd_buf.h"
#include "../core/vos_i18n.h"
#include "../core/vos_log.h"
#include "../drivers/vos_drv_sd.h"

static const size_t SD_PATH_MAX = 100;
static const int SD_LIST_MAX = 32;        // voci per elenco
static const int SD_READ_MAX = 1024;      // byte letti da "read"
static const int SD_WRITE_MAX = 512;      // byte scritti da "write" e "append"

// ---- Controllo dei percorsi ----

// Lettere e numeri ASCII, piu pochi simboli sicuri.
static bool allowedChar(char c) {
  bool asciiAlnum = isalnum((unsigned char)c) && (unsigned char)c < 128;
  bool symbol = c == '_' || c == '-' || c == '.' || c == '/' || c == ' ' || c == '(' || c == ')';
  return asciiAlnum || symbol;
}

// Il carattere in posizione i rende il percorso non valido?
static bool badCharAt(const char* p, size_t n, size_t i) {
  char c = p[i];
  char prev = i > 0 ? p[i - 1] : 0;
  char next = i + 1 < n ? p[i + 1] : 0;

  if (!allowedChar(c)) {
    return true;
  }
  if (c == '/' && next == '/') {
    return true;                                  // "//"
  }
  if (c == '.' && next == '.') {
    return true;                                  // ".."
  }
  if (prev == '/' && c == ' ') {
    return true;                                  // nome che comincia con spazio
  }
  if (c == '/' && (prev == ' ' || prev == '.')) {
    return true;                                  // nome che finisce con spazio o punto (FatFs li taglia) e "/./"
  }

  return false;
}

bool sdPathOk(const char* path, char* err, size_t en) {
  size_t n = path ? strlen(path) : 0;
  bool bad = n == 0 || n > SD_PATH_MAX || path[0] != '/';

  for (size_t i = 0; !bad && i < n; i++) {
    bad = badCharAt(path, n, i);
  }

  // Niente "/" ne spazio ne punto in fondo (tranne la radice).
  if (!bad && n > 1) {
    char last = path[n - 1];
    bad = last == '/' || last == ' ' || last == '.';
  }

  if (bad) {
    sdSetErr(err, en, tr("Percorso non valido: parte da / e usa solo lettere, numeri, spazio e _ - . ( )"));
  }

  return !bad;
}

static bool needMounted(char* err, size_t en) {
  if (drvSdUp()) {
    return true;
  }

  sdSetErr(err, en, tr("La microSD non e montata"));
  return false;
}

// ---- Elenco ----

// Un nome troppo lungo per il buffer (47) o con accenti e simboli si vede ma non si puo aprire da qui.
static bool entryUsable(const DrvSdEntry& entry) {
  if (strlen(entry.name) >= sizeof(entry.name) - 1) {
    return false;
  }

  char path[SD_PATH_MAX + 4];
  char ignored[8];
  snprintf(path, sizeof path, "/%s", entry.name);

  return sdPathOk(path, ignored, sizeof ignored);
}

static void listEntry(Buf& j, const DrvSdEntry& entry) {
  j.put("{\"n\":\"");
  j.esc(entry.name);
  j.fmt("\",\"d\":%d,\"s\":%lu,\"x\":%d}", entry.dir ? 1 : 0, (unsigned long)entry.size, entryUsable(entry) ? 0 : 1);
}

static bool actList(const char* arg, char* out, size_t on, char* err, size_t en) {
  const char* dir = (arg && *arg) ? arg : "/";

  if (strcmp(dir, "/") && !sdPathOk(dir, err, en)) {
    return false;
  }

  DrvSdEntry* entries = (DrvSdEntry*)malloc(SD_LIST_MAX * sizeof(DrvSdEntry));

  if (!entries) {
    sdSetErr(err, en, tr("Memoria insufficiente"));
    return false;
  }

  bool more = false;
  int count = drvSdList(dir, entries, SD_LIST_MAX, more);

  if (count < 0) {
    free(entries);
    sdSetErr(err, en, tr("Cartella non trovata o scheda non leggibile"));
    return false;
  }

  Buf j(out, on);
  j.put("{\"dir\":\"");
  j.esc(dir);
  j.fmt("\",\"more\":%d,\"list\":[", more ? 1 : 0);

  for (int i = 0; i < count; i++) {
    if (i) {
      j.ch(',');
    }

    listEntry(j, entries[i]);
  }

  j.put("]}");
  free(entries);

  if (j.ov) {
    sdSetErr(err, en, tr("Risposta troppo lunga"));
    return false;
  }

  return true;
}

// ---- Lettura ----

// Solo testo semplice: il resto diventa un punto.
static char printableOrDot(uint8_t byte) {
  char c = (char)byte;
  bool printable = (c >= 32 && c < 127) || c == '\n';
  return printable ? c : '.';
}

static bool actRead(const char* path, char* out, size_t on, char* err, size_t en) {
  if (!sdPathOk(path, err, en)) {
    return false;
  }

  uint8_t* data = (uint8_t*)malloc(SD_READ_MAX);

  if (!data) {
    sdSetErr(err, en, tr("Memoria insufficiente"));
    return false;
  }

  int count = drvSdRead(path, data, SD_READ_MAX);

  if (count < 0) {
    free(data);
    sdSetErr(err, en, tr("File non trovato o non leggibile"));
    return false;
  }

  Buf j(out, on);
  j.put("{\"path\":\"");
  j.esc(path);
  j.fmt("\",\"n\":%d,\"text\":\"", count);

  for (int i = 0; i < count; i++) {
    char one[2] = { printableOrDot(data[i]), 0 };
    j.esc(one);
  }

  j.put("\"}");
  free(data);

  if (j.ov) {
    sdSetErr(err, en, tr("Risposta troppo lunga"));
    return false;
  }

  return true;
}

// ---- Scrittura ----

// arg = "percorso,testo".
static bool actWrite(const char* arg, bool append, char* out, size_t on, char* err, size_t en) {
  const char* comma = strchr(arg, ',');
  size_t pathLen = comma ? (size_t)(comma - arg) : strlen(arg);

  if (pathLen > SD_PATH_MAX) {
    sdSetErr(err, en, tr("Percorso non valido: parte da / e usa solo lettere, numeri, spazio e _ - . ( )"));
    return false;
  }

  char path[SD_PATH_MAX + 2];
  memcpy(path, arg, pathLen);
  path[pathLen] = 0;

  if (!sdPathOk(path, err, en)) {
    return false;
  }

  const char* text = comma ? comma + 1 : "";
  size_t textLen = strlen(text);

  if (textLen > SD_WRITE_MAX) {
    snprintf(err, en, tr("Testo troppo lungo (massimo %d caratteri)"), SD_WRITE_MAX);
    return false;
  }

  if (!drvSdWrite(path, (const uint8_t*)text, textLen, append)) {
    sdSetErr(err, en, tr("Scrittura fallita (scheda protetta, piena o tolta?)"));
    return false;
  }

  vlog("SD: %s %s (%u byte)", append ? "aggiunto a" : "scritto", path, (unsigned)textLen);
  snprintf(out, on, "{\"ok\":true,\"n\":%u}", (unsigned)textLen);
  return true;
}

// ---- Cancellazione e cartelle ----

static bool actRemoveOrMkdir(bool remove, const char* path, char* out, size_t on, char* err, size_t en) {
  if (!sdPathOk(path, err, en)) {
    return false;
  }

  bool done = remove ? drvSdRemove(path) : drvSdMkdir(path);

  if (!done) {
    sdSetErr(err, en, remove ? tr("Non riesco a cancellare (e un file?)") : tr("Non riesco a creare la cartella"));
    return false;
  }

  vlog("SD: %s %s", remove ? "cancellato" : "creata la cartella", path);
  snprintf(out, on, "{\"ok\":true}");
  return true;
}

// ---- Punto d'ingresso ----

static bool roleDenied(int role, int needed, char* err, size_t en) {
  if (role >= needed) {
    return false;
  }

  sdSetErr(err, en, tr("Il tuo ruolo non permette questo comando"));
  return true;
}

// Azioni per tutti gli Operatori: stato ed elenco.
static bool actReadOnly(const char* action, const char* arg, int role, char* out, size_t on, char* err, size_t en) {
  if (roleDenied(role, ROLE_OPER, err, en)) {
    return false;
  }
  if (!strcmp(action, "state")) {
    sdStatusJson(out, on);
    return true;
  }

  return needMounted(err, en) && actList(arg, out, on, err, en);
}

// Azioni solo per l'Admin: i file della scheda possono contenere dati privati.
static bool actAdmin(const char* action, const char* arg, char* out, size_t on, char* err, size_t en) {
  if (!strcmp(action, "mount")) {
    if (!sdMount(err, en)) {
      return false;
    }

    sdStatusJson(out, on);
    return true;
  }

  if (!strcmp(action, "unmount")) {
    if (!sdUnmount(err, en)) {
      return false;
    }

    sdStatusJson(out, on);
    return true;
  }

  if (!needMounted(err, en)) {
    return false;
  }
  if (!strcmp(action, "test")) {
    return sdProve(out, on, err, en);
  }
  if (!strcmp(action, "read")) {
    return actRead(arg, out, on, err, en);
  }
  if (!strcmp(action, "write")) {
    return actWrite(arg, false, out, on, err, en);
  }
  if (!strcmp(action, "append")) {
    return actWrite(arg, true, out, on, err, en);
  }
  if (!strcmp(action, "rm")) {
    return actRemoveOrMkdir(true, arg, out, on, err, en);
  }
  if (!strcmp(action, "mkdir")) {
    return actRemoveOrMkdir(false, arg, out, on, err, en);
  }

  sdSetErr(err, en, tr("Azione non valida"));
  return false;
}

static bool sdActInner(const char* action, const char* arg, int role, char* out, size_t on, char* err, size_t en) {
  if (on) {
    out[0] = 0;
  }
  if (!action) {
    action = "";
  }
  if (!arg) {
    arg = "";
  }

  if (!sdConfigured()) {
    sdSetErr(err, en, tr("Nessuna microSD configurata: aggiungila da Periferiche"));
    return false;
  }

  if (!strcmp(action, "state") || !strcmp(action, "ls")) {
    return actReadOnly(action, arg, role, out, on, err, en);
  }
  if (roleDenied(role, ROLE_ADMIN, err, en)) {
    return false;
  }

  return actAdmin(action, arg, out, on, err, en);
}

// Il punto d'ingresso vero: conta le azioni in corso, cosi chi smonta (vesevos-drive_manager) sa se qualcuno sta usando la scheda.
bool sdAct(const char* action, const char* arg, int role, char* out, size_t on, char* err, size_t en) {
  sdOpEnter();
  bool done = sdActInner(action, arg, role, out, on, err, en);
  sdOpLeave();
  return done;
}

#endif
