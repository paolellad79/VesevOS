// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_drive.cpp
// Comando "vesevos-drive_manager": i volumi (memoria interna e microSD) in testo.
//
//   vesevos-drive_manager [list]     elenco dei volumi: stato, formato, dimensione, usato, libero, frequenza SPI (tutti i ruoli)
//   vesevos-drive_manager status     come list; l'Admin vede in piu la riga di "sd status"
//   vesevos-drive_manager mount      = "sd mount"     (solo Admin)
//   vesevos-drive_manager unmount    = "sd unmount"   (solo Admin; chiede conferma se la scheda e in uso)
//   vesevos-drive_manager test       = "sd test"      (solo Admin)
//
//   vesevos-drive_manager format microsd FORMATTA [--attendi]    formatta la microSD in FAT32 (solo Admin; CANCELLA TUTTO; la parola e la conferma). Mai la memoria interna.
//     Torna subito (formattazione in un task a parte, devices/vos_sd_fmt.cpp); si guarda con "sd status". --attendi: resta in attesa (al massimo 60 s) e scrive il tempo trascorso.
// La formattazione la fa la primitiva sdFormat (devices/vos_sd.cpp); decisione dell'utente del 31/10/2026 (prima la microSD non si formattava mai).
// mount, unmount, test e la riga di status non hanno codice qui: ricostruiscono la riga del comando "sd" e la fanno passare dal PERCORSO
// NORMALE della shell (shellExecSess). "sd" e il comando del driver microSD (scritto in Berry, nel suo file in /devs): senza il driver non c'e
// la periferica e queste azioni lo dicono. Stessa tabella dei ruoli e stessi controlli del driver.
// L'elenco legge soltanto variabili e un settore della scheda (per vedere se e stata tolta): e veloce e non blocca la pagina.
#include "vos_shell_int.h"
#include "vos_shell_ask.h"
#include "../packages/vos_drvcmd.h"
#include "vos_shell_rep.h"
#if VOS_WITH_SD
#include "../devices/vos_sd.h"
#include "../drivers/vos_drv_task.h"
#include "../drivers/vos_drv_time.h"
#endif

namespace {

// Una riga della tabella dei volumi. sizes = false per i volumi non montati (dimensioni "-").
void volumeRow(OutSink& o, const char* name, const char* mount, const char* fs, uint32_t totalKb, uint32_t usedKb, bool sizes, const char* state) {
  char total[16] = "-", used[16] = "-", avail[16] = "-", pct[8] = "-";
  if (sizes) {
    if (usedKb > totalKb) usedKb = totalKb;
    repBytes((uint64_t)totalKb * 1024ULL, total, sizeof total);
    repBytes((uint64_t)usedKb * 1024ULL, used, sizeof used);
    repBytes((uint64_t)(totalKb - usedKb) * 1024ULL, avail, sizeof avail);
    if (totalKb) snprintf(pct, sizeof pct, "%lu%%", (unsigned long)(((uint64_t)usedKb * 100 + totalKb - 1) / totalKb));
  }
  o.printf("%-9s %-9s %-9s %9s %9s %9s %5s  %s\r\n", name, mount, fs, total, used, avail, pct, state);
}

#if VOS_WITH_SD
const char* sdStateWord(uint8_t state) {
  switch (state) {
    case SD_ST_MOUNTED:    return tr("montata");
    case SD_ST_NO_CARD:    return tr("non presente");
    case SD_ST_BAD_FORMAT: return tr("formato non supportato");
    default:               return tr("non montata");
  }
}
#endif

void volumes(OutSink& o) {
  o.printf("%-9s %-9s %-9s %9s %9s %9s %5s  %s\r\n", tr("Volume"), tr("Punto"), tr("Formato"), tr("Dimens."), tr("Usato"), tr("Libero"), tr("Uso%"), tr("Stato"));
  volumeRow(o, "littlefs", "/", "LittleFS", (uint32_t)(drvFsTotal() / 1024), (uint32_t)(drvFsUsed() / 1024), true, tr("montato"));
#if VOS_WITH_SD
  SdVolume v;
  if (!sdVolume(v)) {
    o.println(tr("microSD: non configurata. Installa il driver microSD da Periferiche > Schede modulo"));
    return;
  }
  bool up = v.state == SD_ST_MOUNTED;
  SdFmtInfo f;
  sdFormatInfo(f);
  char fmt[32];
  const char* state = sdStateWord(v.state);
  if (f.running) {
    snprintf(fmt, sizeof fmt, "%s %02u:%02u", tr("formattazione"), (unsigned)(f.secs / 60), (unsigned)(f.secs % 60));
    state = fmt;
  }
  volumeRow(o, "microsd", VOS_SD_MOUNT_POINT, up ? v.fs : "-", v.totalKb, v.usedKb, up, state);
  o.println(trf("microsd: frequenza SPI %lu kHz; si usa con il comando sd", (unsigned long)v.khz));
  if (v.state == SD_ST_BAD_FORMAT) o.println(tr("Formato non supportato: serve FAT32. Formattala dal pannello della microSD o con: vesevos-drive_manager format microsd"));
#else
  o.println(tr("Questo firmware non ha la microSD"));
#endif
}

// Ci sono parole in piu dopo il sottocomando (a parte la conferma " yes" messa da askConfirm)?
bool extraWords(const char* line) {
  Word x = argAt(line, 2);
  if (x.empty()) return false;
  x.lower();
  return !(askConfirmed(line) && x == "yes" && argAt(line, 3).empty());
}

#if VOS_WITH_SD
// Ricostruisce la riga del comando "sd" e la fa passare dal percorso normale della shell.
void forwardSd(const char* action, OutSink& o, int role, uint32_t sess) {
  if (!drvCmdHas("sd")) { o.println(tr("Nessun driver per la microSD: caricalo da Dispositivi > Periferiche > Schede modulo")); return; }
  char fwd[112];
  snprintf(fwd, sizeof fwd, "sd %s", action);
  shellExecSess(fwd, o, role, sess);
}
#endif

#if VOS_WITH_SD
// Aspetta la fine della formattazione, al massimo FMT_WAIT_S secondi (la shell web risponde a comando finito: un attesa lunga terrebbe occupato il server).
// Una riga col tempo ogni FMT_STEP_S secondi: sulla seriale si vedono man mano, sulla shell web arrivano tutte insieme alla fine.
void waitFormat(OutSink& o) {
  const uint32_t FMT_WAIT_S = 60, FMT_STEP_S = 5;
  SdFmtInfo f;
  uint32_t shown = 0;
  for (uint32_t waited = 0; waited < FMT_WAIT_S * 2; waited++) {
    sdFormatInfo(f);
    if (!f.running) break;
    if (f.secs >= shown + FMT_STEP_S) { shown = f.secs; o.println(trf("Formattazione in corso... %02u:%02u", (unsigned)(f.secs / 60), (unsigned)(f.secs % 60))); }
    drvDelayMs(500);
    drvTaskPause();
  }
  sdFormatInfo(f);
  if (f.running) o.println(trf("Formattazione ancora in corso (%02u:%02u). Controlla con: sd status", (unsigned)(f.secs / 60), (unsigned)(f.secs % 60)));
  else if (f.last == 1) o.println(trf("microSD formattata in FAT32 in %02u:%02u", (unsigned)(f.lastSecs / 60), (unsigned)(f.lastSecs % 60)));
  else o.println(f.err[0] ? f.err : tr("Formattazione non riuscita"));
}

// vesevos-drive_manager format microsd FORMATTA [--attendi]: senza la parola di conferma dice cosa succede e non fa niente.
// La formattazione parte in un task a parte e il comando torna subito; con --attendi resta in attesa e scrive il tempo trascorso.
void formatCmd(const char* line, OutSink& o, int role) {
  if (role != ROLE_ADMIN) { o.println(tr("Serve il livello Admin")); return; }
  Word vol = argAt(line, 2), word = argAt(line, 3), opt = argAt(line, 4);
  vol.lower();
  opt.lower();
  bool wait = opt == "--attendi";
  if (vol == "littlefs") { o.println(tr("La memoria interna non si formatta da qui")); return; }
  if (!(vol == "microsd") || !argAt(line, 5).empty() || !(opt.empty() || wait)) { o.println(trf("Uso: vesevos-drive_manager format microsd %s [--attendi]", SD_FORMAT_WORD)); return; }
  if (!sdConfigured()) { o.println(tr("microSD: non configurata. Installa il driver microSD da Periferiche > Schede modulo")); return; }
  if (!(word == SD_FORMAT_WORD)) {
    o.println(trf("ATTENZIONE: la formattazione CANCELLA TUTTO dalla microSD e la rifa in FAT32. Per confermare scrivi: vesevos-drive_manager format microsd %s", SD_FORMAT_WORD));
    return;
  }
  char err[SD_ERR_MAX];
  if (!sdFormatStart(err, sizeof err)) { o.println(err); return; }
  o.println(tr("Formattazione avviata. Controlla con: sd status"));
  if (wait) waitFormat(o);
}
#else
void formatCmd(const char*, OutSink& o, int) { o.println(tr("Questo firmware non ha la microSD")); }
#endif

}  // namespace

void shx::shDriveManager(const char* line, OutSink& o, int role, uint32_t sess) {
  Word sub = argAt(line, 1);
  sub.lower();
  if (sub == "yes" && askConfirmed(line)) sub = Word();             // "vesevos-drive_manager yes" non significa niente
  if (sub.empty() || sub == "list") {
    if (extraWords(line)) { cmdUsage(o, "vesevos-drive_manager"); return; }
    volumes(o);
    return;
  }
  if (sub == "format") { formatCmd(line, o, role); return; }
  bool change = sub == "mount" || sub == "unmount" || sub == "test";
  if (!change && sub != "status") { cmdUsage(o, "vesevos-drive_manager"); return; }
  if (extraWords(line)) { cmdUsage(o, "vesevos-drive_manager"); return; }
  if (change && role != ROLE_ADMIN) { o.println(tr("Serve il livello Admin")); return; }
#if VOS_WITH_SD
  if (sub == "status") {
    volumes(o);
    if (role == ROLE_ADMIN && drvCmdHas("sd")) forwardSd("status", o, role, sess);
    return;
  }
  if (!sdConfigured()) { o.println(tr("microSD: non configurata. Installa il driver microSD da Periferiche > Schede modulo")); return; }
  forwardSd(sub.c_str(), o, role, sess);
  if (sub != "test") volumes(o);                                     // mount e unmount: si vede com'e finita
#else
  if (sub == "status") { volumes(o); return; }
  o.println(tr("Questo firmware non ha la microSD"));
#endif
}
