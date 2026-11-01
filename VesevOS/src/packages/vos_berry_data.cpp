// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_berry_data.cpp: la CARTELLA DEI DATI dell'app Berry. Il lavoro vero sta in vos_prog_data.cpp (uguale per le app ELF, vedi vos_prim_files.cpp).
//
//   data_dir()                          ->  testo: la cartella dei dati ("/var/lib/<nome>" o "/media/sd/app/<nome>", la crea se manca)
//   data_write("file", dati [, append]) ->  true; dati = testo o bytes, al massimo 1024 byte per chiamata
//   data_read("file" [, offset [, n]])  ->  bytes (al massimo 1024 per chiamata; vuoto = oltre la fine), nil se il file non c'e
//   data_rm("file")                     ->  true
// Dove stanno i dati (flash o microSD) lo sceglie l'utente dalla pagina App: l'app non lo sa. Solo le app con data=1 nel manifest e "files" in use=
// hanno queste funzioni. Il nome del file e semplice (a-z A-Z 0-9 _ . -, al massimo 40 caratteri), nessun sotto-percorso: l'app non esce dalla sua cartella.
// Nessun oggetto con distruttore resta vivo quando si chiama be_raise (longjmp): i buffer sono statici (una sola esecuzione alla volta).
#include "vos_berry_int.h"
#include "../core/vos_common.h"
#if VOS_WITH_BERRY
#include "../third_party/berry/src/berry.h"
#include <stdio.h>
#include <string.h>
#include "vos_prog_data.h"
#include "../core/vos_files.h"

static char    g_owner[25];              // l'app che ha queste funzioni
static ErrBuf  g_er;                     // statico: con be_raise (longjmp) nessun oggetto con distruttore resta vivo
static uint8_t g_buf[PD_IO_MAX];
static char    g_msg[96];

static void stopCheck(bvm* vm) { if (g_stop) be_raise(vm, "stopped_error", "script fermato"); }

static const char* motivo(int code) {
  switch (code) {
    case PD_E_NOSD:       return "la microSD scelta per i dati non e montata";
    case PD_E_PERCORSO:   return "nome di file non ammesso (a-z A-Z 0-9 _ . -, massimo 40 caratteri)";
    case PD_E_PIENO:      return "spazio esaurito";
    case PD_E_VIETATO:    return "questa app non salva dati (manca data=1)";
    default:              return "errore del file system";
  }
}

// Il nome del file (primo argomento).
static const char* fileArg(bvm* vm, const char* fn) {
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) { snprintf(g_msg, sizeof g_msg, "%s(\"file\", ...): serve il nome del file (testo)", fn); be_raise(vm, "type_error", g_msg); }
  return be_tostring(vm, 1);
}

static int bfDataDir(bvm* vm) {
  berryNeedFunc(vm, "files");
  static char dir[FS_PATH_MAX + 1];
  stopCheck(vm);
  if (!progDataDir(PK_APP_B, g_owner, dir, sizeof dir, g_er)) { snprintf(g_msg, sizeof g_msg, "data_dir: %s", g_er.c_str()); be_raise(vm, "io_error", g_msg); }
  be_pushstring(vm, dir);
  be_return(vm);
}

static int bfDataWrite(bvm* vm) {
  berryNeedFunc(vm, "files");
  const char* file = fileArg(vm, "data_write");
  const void* p = nullptr; size_t n = 0;
  if (be_top(vm) >= 2 && be_isstring(vm, 2)) { p = be_tostring(vm, 2); n = (size_t)be_strlen(vm, 2); }
  else if (be_top(vm) >= 2 && be_isbytes(vm, 2)) p = be_tobytes(vm, 2, &n);
  if (!p) be_raise(vm, "type_error", "data_write(\"file\", dati [, append]): dati = testo o bytes");
  if (n > PD_IO_MAX) be_raise(vm, "value_error", "data_write: al massimo 1024 byte per chiamata");
  bool append = be_top(vm) >= 3 && ((be_isbool(vm, 3) && be_tobool(vm, 3)) || (be_isint(vm, 3) && be_toint(vm, 3) != 0));
  stopCheck(vm);
  int r = progDataWrite(PK_APP_B, g_owner, file, (const uint8_t*)p, n, append);
  if (r < 0) { snprintf(g_msg, sizeof g_msg, "data_write: %s", motivo(r)); be_raise(vm, "io_error", g_msg); }
  be_pushbool(vm, true);
  be_return(vm);
}

static int bfDataRead(bvm* vm) {
  berryNeedFunc(vm, "files");
  const char* file = fileArg(vm, "data_read");
  long off = 0, n = PD_IO_MAX;
  if (be_top(vm) >= 2) { if (!be_isint(vm, 2) || be_toint(vm, 2) < 0) be_raise(vm, "value_error", "data_read: offset = numero intero >= 0"); off = (long)be_toint(vm, 2); }
  if (be_top(vm) >= 3) { if (!be_isint(vm, 3) || be_toint(vm, 3) < 0 || be_toint(vm, 3) > PD_IO_MAX) be_raise(vm, "value_error", "data_read: n va da 0 a 1024"); n = (long)be_toint(vm, 3); }
  stopCheck(vm);
  if (n == 0) { be_pushbytes(vm, g_buf, 0); be_return(vm); }
  int r = progDataRead(PK_APP_B, g_owner, file, (uint32_t)off, g_buf, (size_t)n);
  if (r == PD_E_NONTROVATO) be_return_nil(vm);
  if (r < 0) { snprintf(g_msg, sizeof g_msg, "data_read: %s", motivo(r)); be_raise(vm, "io_error", g_msg); }
  be_pushbytes(vm, g_buf, (size_t)r);
  be_return(vm);
}

static int bfDataRm(bvm* vm) {
  berryNeedFunc(vm, "files");
  const char* file = fileArg(vm, "data_rm");
  stopCheck(vm);
  int r = progDataRemove(PK_APP_B, g_owner, file);
  if (r < 0 && r != PD_E_NONTROVATO) { snprintf(g_msg, sizeof g_msg, "data_rm: %s", motivo(r)); be_raise(vm, "io_error", g_msg); }
  be_pushbool(vm, r == 0);
  be_return(vm);
}

void berryDataRegister(bvm* vm, const char* appName) {
  snprintf(g_owner, sizeof g_owner, "%s", appName ? appName : "");
  be_regfunc(vm, "data_dir", bfDataDir);
  be_regfunc(vm, "data_write", bfDataWrite);
  be_regfunc(vm, "data_read", bfDataRead);
  be_regfunc(vm, "data_rm", bfDataRm);
}
#else
struct bvm;
void berryDataRegister(bvm*, const char*) {}
#endif
