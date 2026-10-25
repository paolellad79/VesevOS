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
// Non c'e "format": VesevOS non formatta mai la microSD (si fa sul computer, in FAT32) e exFAT e vietato dal progetto.
// mount, unmount, test e la riga di status non hanno codice qui: ricostruiscono la riga del comando "sd" e la fanno passare dal PERCORSO
// NORMALE della shell (shellExecSess): stessa tabella dei ruoli (sd e solo Admin), stessi controlli di sdAct, stessi messaggi.
// L'elenco legge soltanto variabili e un settore della scheda (per vedere se e stata tolta): e veloce e non blocca la pagina.
#include "vos_shell_int.h"
#include "vos_shell_ask.h"
#include "vos_shell_rep.h"
#if VOS_WITH_SD
#include "../devices/vos_sd.h"
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
  o.printf("%-9s %-5s %-9s %9s %9s %9s %5s  %s\r\n", name, mount, fs, total, used, avail, pct, state);
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
  o.printf("%-9s %-5s %-9s %9s %9s %9s %5s  %s\r\n", tr("Volume"), tr("Punto"), tr("Formato"), tr("Dimens."), tr("Usato"), tr("Libero"), tr("Uso%"), tr("Stato"));
  volumeRow(o, "littlefs", "/", "LittleFS", (uint32_t)(drvFsTotal() / 1024), (uint32_t)(drvFsUsed() / 1024), true, tr("montato"));
#if VOS_WITH_SD
  SdVolume v;
  if (!sdVolume(v)) {
    o.println(tr("microSD: non configurata. Aggiungi la periferica 'sd' dalla pagina Periferiche"));
    return;
  }
  bool up = v.state == SD_ST_MOUNTED;
  volumeRow(o, "microsd", "/sd", up ? v.fs : "-", v.totalKb, v.usedKb, up, sdStateWord(v.state));
  o.println(trf("microsd: frequenza SPI %lu kHz; si usa con il comando sd", (unsigned long)v.khz));
  if (v.state == SD_ST_BAD_FORMAT) o.println(tr("Serve FAT32: formatta la scheda sul computer (VesevOS non formatta mai da solo)"));
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
  char fwd[112];
  snprintf(fwd, sizeof fwd, "sd %s", action);
  shellExecSess(fwd, o, role, sess);
}
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
  if (sub == "format") {
    o.println(tr("La formattazione non e nel firmware: VesevOS non formatta mai la microSD. Formattala in FAT32 sul computer (exFAT non e supportato)"));
    return;
  }
  bool change = sub == "mount" || sub == "unmount" || sub == "test";
  if (!change && sub != "status") { cmdUsage(o, "vesevos-drive_manager"); return; }
  if (extraWords(line)) { cmdUsage(o, "vesevos-drive_manager"); return; }
  if (change && role != ROLE_ADMIN) { o.println(tr("Serve il livello Admin")); return; }
#if VOS_WITH_SD
  if (sub == "status") {
    volumes(o);
    if (role == ROLE_ADMIN) forwardSd("status", o, role, sess);
    return;
  }
  if (!sdConfigured()) { o.println(tr("microSD: non configurata. Aggiungi la periferica 'sd' dalla pagina Periferiche")); return; }
  if (sub == "unmount" && sdInUse() && !askConfirmed(line)) {
    askConfirm(o, line, tr("La microSD e in uso (c'e un'operazione in corso). Smonto lo stesso?"));
    return;
  }
  forwardSd(sub.c_str(), o, role, sess);
  if (sub != "test") volumes(o);                                     // mount e unmount: si vede com'e finita
#else
  if (sub == "status") { volumes(o); return; }
  o.println(tr("Questo firmware non ha la microSD"));
#endif
}
