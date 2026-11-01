// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_berry_sd.cpp: funzioni Berry della microSD. I file veri li tocca devices/vos_sd_io.cpp.
//
//   sd_status()                         ->  {"state": 1..4 (come la Home), "fs": testo, "total_kb", "used_kb", "khz",
//                                            "fmt_run": 1 se si sta formattando, "fmt_secs": secondi trascorsi, "fmt_last": 0 mai/1 riuscita/2 non riuscita,
//                                            "fmt_last_secs": durata dell'ultima, "fmt_err": motivo se non riuscita}
//   sd_read("percorso" [, offset [, n]])  ->  bytes (al massimo 1024 per chiamata; vuoto = oltre la fine), nil se non c'e o non lo puoi leggere
//   sd_mount() / sd_eject()             ->  true = fatto; sd_eject rifiuta (false) se la scheda e in uso
//   sd_format("FORMATTA")               ->  true = formattazione AVVIATA (CANCELLA TUTTO, solo Admin, FAT32); torna subito, la fine si vede con sd_status() (fmt_run, fmt_last).
//                                            Senza la parola di conferma e un errore
//   sd_write("percorso", dati [, append])  ->  true; dati = testo o bytes, al massimo 1024 byte per chiamata
//   sd_rm("percorso") / sd_mkdir("percorso")  ->  true; rm toglie un file o una cartella vuota, mai la radice
//
// PERMESSI
//   - Gli script (.vsb e "berry <codice>") le hanno sempre; le app solo con "sd" in use= del manifest.
//   - sd_status e sd_read per tutti. Il resto (registrato sempre, ma "permesso negato" a chi non puo) solo se lo script e stato lanciato dall'Admin, o se l'app ha who=admin (la avvia solo l'Admin):
//     una regola, MQTT o la rete tra schede non possono scrivere ne smontare. Chiavi, certificati e copie .vbk (pathIsSecret) li legge
//     solo l'Admin; i file di sistema non si leggono e non si toccano mai.
// REGOLE
//   - Il percorso passa da pathResolve (".." risolto, "/sd" = "/media/sd") e DEVE stare sotto /media/sd; poi sdPathOk.
//   - Un blocco alla volta, al massimo SD_IO_MAX byte: lo script che vuole un file grande lo legge/scrive a pezzi e fa sleep() tra i giri.
//   - Nessun oggetto con distruttore resta vivo quando si chiama be_raise (longjmp): i buffer sono statici (una sola esecuzione alla volta).
#include "vos_berry_int.h"
#include "../core/vos_common.h"
#if VOS_WITH_BERRY
#include "../third_party/berry/src/berry.h"
#include <stdio.h>
#include <string.h>
#include "../core/vos_files.h"
#include "../core/vos_i18n.h"
#include "../core/vos_log.h"
#include "../core/vos_pathres.h"
#include "../core/vos_paths.h"
#include "../devices/vos_sd.h"

#if VOS_WITH_SD
static uint8_t g_buf[SD_IO_MAX];         // blocco letto o da scrivere
static char    g_err[SD_ERR_MAX];        // motivo dell'ultimo errore (testo tradotto dalle funzioni della scheda)
static bool    g_secretsOk = false;      // chi ha lanciato lo script e l'Admin: puo leggere chiavi e certificati (e usare le funzioni di scrittura)

static void stopCheck(bvm* vm) { if (g_stop) be_raise(vm, "stopped_error", "script fermato"); }

// Le funzioni di scrittura esistono SEMPRE (un file di comandi che le nomina deve compilare anche per un sottocomando di sola lettura):
// chi non e Admin (o non ha who=admin) riceve "permesso negato" solo quando le chiama davvero.
static void needAdmin(bvm* vm, const char* fn) {
  if (g_secretsOk) return;
  snprintf(g_err, sizeof g_err, "%s: %s", fn, tr("permesso negato (serve l'Admin)"));
  be_raise(vm, "permission_error", g_err);
}

// Il percorso (primo argomento), risolto dalla radice. Torna il percorso sulla scheda ("/dir/file"); clean = il percorso intero (almeno FS_PATH_MAX + 1 byte).
static const char* pathArg(bvm* vm, const char* fn, char* clean) {
  if (be_top(vm) < 1 || !be_isstring(vm, 1)) { snprintf(g_err, sizeof g_err, "%s(\"percorso\", ...): serve un percorso (testo)", fn); be_raise(vm, "type_error", g_err); }
  if (!pathResolve("/", be_tostring(vm, 1), clean, FS_PATH_MAX + 1)) be_raise(vm, "value_error", "percorso non valido");
  const char* rel = pathSdRel(clean);
  if (!rel) be_raise(vm, "value_error", "il percorso deve stare sotto /media/sd");
  if (!sdPathOk(rel, g_err, sizeof g_err)) be_raise(vm, "value_error", g_err);
  return rel;
}

// Un numero intero opzionale (argomento i): torna def se manca; errore se non e un intero tra lo e hi.
static long intArg(bvm* vm, int i, const char* fn, long def, long lo, long hi) {
  if (be_top(vm) < i) return def;
  if (!be_isint(vm, i)) { snprintf(g_err, sizeof g_err, "%s: l'argomento %d deve essere un numero intero", fn, i); be_raise(vm, "type_error", g_err); }
  long v = (long)be_toint(vm, i);
  if (v < lo || v > hi) { snprintf(g_err, sizeof g_err, "%s: l'argomento %d va da %ld a %ld", fn, i, lo, hi); be_raise(vm, "value_error", g_err); }
  return v;
}

// Per scrivere o cancellare: i file di sistema non si toccano mai.
static void notSystem(bvm* vm, const char* clean) {
  if (pathIsSystemFile(clean)) be_raise(vm, "value_error", "file di sistema: non si tocca");
}

static int bfSdStatus(bvm* vm) {
  berryNeedFunc(vm, "sd");
  SdVolume v;
  if (!sdVolume(v)) memset(&v, 0, sizeof v);                 // nessuna periferica "sd": stato 0
  be_newmap(vm);
  berryPutInt(vm, "state", (long)v.state);
  berryPutText(vm, "fs", v.fs);
  berryPutInt(vm, "total_kb", (long)v.totalKb);
  berryPutInt(vm, "used_kb", (long)v.usedKb);
  berryPutInt(vm, "khz", (long)v.khz);
  SdFmtInfo f;
  sdFormatInfo(f);
  berryPutInt(vm, "fmt_run", f.running ? 1 : 0);
  berryPutInt(vm, "fmt_secs", (long)f.secs);
  berryPutInt(vm, "fmt_last", (long)f.last);
  berryPutInt(vm, "fmt_last_secs", (long)f.lastSecs);
  berryPutText(vm, "fmt_err", f.err);
  berryWrap(vm, "map");
  be_return(vm);
}

static int bfSdRead(bvm* vm) {
  berryNeedFunc(vm, "sd");
  char clean[FS_PATH_MAX + 1];
  const char* rel = pathArg(vm, "sd_read", clean);
  long off = intArg(vm, 2, "sd_read", 0, 0, 0x7FFFFFFFL);
  long n = intArg(vm, 3, "sd_read", SD_IO_MAX, 0, SD_IO_MAX);
  stopCheck(vm);
  if (pathIsSystemFile(clean) || (pathIsSecret(clean) && !g_secretsOk)) be_return_nil(vm);   // come se non esistesse
  if (!sdUp()) be_raise(vm, "io_error", sdFormatting() ? "microSD occupata (formattazione)" : "la microSD non e montata");
  int got = sdReadAt(rel, (uint32_t)off, g_buf, (size_t)n, g_err, sizeof g_err);
  if (got < 0) be_return_nil(vm);                            // non c'e (o non si apre)
  be_pushbytes(vm, g_buf, (size_t)got);
  be_return(vm);
}

static int bfSdMount(bvm* vm) {
  berryNeedFunc(vm, "sd");
  needAdmin(vm, "sd_mount");
  stopCheck(vm);
  bool ok = sdMount(g_err, sizeof g_err);
  if (!ok) vlog("SCRIPT: sd_mount: %s", g_err);
  be_pushbool(vm, ok);
  be_return(vm);
}

// Smonta come "vesevos-drive_manager unmount" (stessa sdUnmount), ma senza chiedere conferma: se la scheda e in uso dice di no.
static int bfSdEject(bvm* vm) {
  berryNeedFunc(vm, "sd");
  needAdmin(vm, "sd_eject");
  stopCheck(vm);
  bool ok = !sdInUse() && sdUnmount(g_err, sizeof g_err);
  if (!ok) vlog("SCRIPT: sd_eject non eseguito: %s", sdInUse() ? "la microSD e in uso" : g_err);
  be_pushbool(vm, ok);
  be_return(vm);
}

// Avvia la formattazione della microSD in FAT32 (CANCELLA TUTTO). Serve la parola di conferma: sd_format("FORMATTA"). Solo Admin. Torna subito (task a parte).
static int bfSdFormat(bvm* vm) {
  berryNeedFunc(vm, "sd");
  needAdmin(vm, "sd_format");
  if (be_top(vm) < 1 || !be_isstring(vm, 1) || strcmp(be_tostring(vm, 1), SD_FORMAT_WORD)) be_raise(vm, "value_error", "sd_format(\"" SD_FORMAT_WORD "\"): serve la parola di conferma");
  stopCheck(vm);
  bool ok = sdFormatStart(g_err, sizeof g_err);
  vlog("SCRIPT: sd_format %s", ok ? "avviato" : "non eseguito");
  be_pushbool(vm, ok);
  be_return(vm);
}

static int bfSdWrite(bvm* vm) {
  berryNeedFunc(vm, "sd");
  needAdmin(vm, "sd_write");
  char clean[FS_PATH_MAX + 1];
  const char* rel = pathArg(vm, "sd_write", clean);
  const void* p = nullptr; size_t n = 0;
  if (be_top(vm) >= 2 && be_isstring(vm, 2)) { p = be_tostring(vm, 2); n = (size_t)be_strlen(vm, 2); }
  else if (be_top(vm) >= 2 && be_isbytes(vm, 2)) p = be_tobytes(vm, 2, &n);
  if (!p) be_raise(vm, "type_error", "sd_write(\"percorso\", dati [, append]): dati = testo o bytes");
  if (n > SD_IO_MAX) be_raise(vm, "value_error", "sd_write: al massimo 1024 byte per chiamata");
  bool append = be_top(vm) >= 3 && ((be_isbool(vm, 3) && be_tobool(vm, 3)) || (be_isint(vm, 3) && be_toint(vm, 3) != 0));
  notSystem(vm, clean);
  stopCheck(vm);
  if (!sdWriteBytes(rel, (const uint8_t*)p, n, append, g_err, sizeof g_err)) be_raise(vm, "io_error", g_err);
  be_pushbool(vm, true);
  be_return(vm);
}

static int bfSdRm(bvm* vm) {
  berryNeedFunc(vm, "sd");
  needAdmin(vm, "sd_rm");
  char clean[FS_PATH_MAX + 1];
  const char* rel = pathArg(vm, "sd_rm", clean);
  notSystem(vm, clean);
  stopCheck(vm);
  if (!sdRemove(rel, g_err, sizeof g_err)) be_raise(vm, "io_error", g_err);
  be_pushbool(vm, true);
  be_return(vm);
}

static int bfSdMkdir(bvm* vm) {
  berryNeedFunc(vm, "sd");
  needAdmin(vm, "sd_mkdir");
  char clean[FS_PATH_MAX + 1];
  const char* rel = pathArg(vm, "sd_mkdir", clean);
  notSystem(vm, clean);
  stopCheck(vm);
  if (!sdMakeDir(rel, g_err, sizeof g_err)) be_raise(vm, "io_error", g_err);
  be_pushbool(vm, true);
  be_return(vm);
}

void berrySdRegister(bvm* vm, bool app, bool admin) {
  (void)app;                                                   // per le app e per gli script la regola e la stessa: conta solo admin
  g_secretsOk = admin;
  be_regfunc(vm, "sd_status", bfSdStatus);
  be_regfunc(vm, "sd_read", bfSdRead);
  be_regfunc(vm, "sd_mount", bfSdMount);   be_regfunc(vm, "sd_eject", bfSdEject);   be_regfunc(vm, "sd_format", bfSdFormat);
  be_regfunc(vm, "sd_write", bfSdWrite);   be_regfunc(vm, "sd_rm", bfSdRm);
  be_regfunc(vm, "sd_mkdir", bfSdMkdir);
}
#else
void berrySdRegister(bvm*, bool, bool) {}
#endif
#else
struct bvm;
void berrySdRegister(bvm*, bool, bool) {}
#endif
