// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_cpu.cpp
// Comando "cpu" nella forma finale e i suoi alias di comando "lscpu" e "vesevos-cpuinfo". Il motore non cambia: scelta e parametri
// stanno in core/vos_cpuset.cpp, il tetto in core/vos_cpupm.cpp.
//
//   cpu                         riepilogo: governor, frequenza e tetto, motivo, temperatura, carico per core
//   cpu info [-v]               chip, frequenze ammesse, memoria, ESP-IDF (con -v anche parametri, soglie termiche, cambi di tetto)
//   cpu governor [nome]         senza nome mostra; con nome: ondemand | conservative | schedutil | fixed <80|160|240>   (cambiare: solo Admin)
//   cpu freq [80|160|240]       senza numero mostra; con numero = velocita fissa   (cambiare: solo Admin)
//   cpu why                     perche il tetto e quello (motivo, parametri, cambi, prenotazioni)
//   cpu set up|down|hold <n>    parametri dei governor (solo Admin)
//   VECCHIE FORME (alias, stessi valori salvati): cpu 80|160|240|ondemand|conservative|schedutil|auto
//   lscpu [-v]                  = cpu info
//   vesevos-cpuinfo [-v]        = riepilogo di "cpu" e poi "cpu info"
//
// "fixed" e il nome del governor che tiene una frequenza fissa (in una shell classica sarebbe "userspace"): e una scelta nostra. Nella config non esiste
// un governor "fixed": fisso vuol dire cfg.cpuMhz diverso da zero, come prima.
// Chi puo fare cosa lo decide la tabella dei ruoli (vos_shell_roles.cpp): qui si controlla solo il ruolo per le forme che CAMBIANO qualcosa.
#include "vos_shell_int.h"
#include "../core/vos_cpupm.h"
#include "../core/vos_cpugov.h"
#include "../core/vos_cpuboost.h"
#include "../drivers/vos_drv_cpu.h"
#include "../drivers/vos_drv_heap.h"
#include "../drivers/vos_drv_sys.h"

namespace {

// Il governor com'e mostrato: "fixed" se la frequenza e fissa, altrimenti il nome del governor.
const char* governorShown() { return cfg.cpuMhz != 0 ? "fixed" : cpuGovName((CpuGov)cfg.cpuGov); }

void tempLine(char* out, size_t n) {
  char tt[16];
  fmtTemp(sysCpuTemp(), tt, sizeof tt);
  snprintf(out, n, "%s (%s)", tt, cpuPmThermalText(cpuPmThermal()));
}

// ---- cpu (riepilogo) ----
void cpuSummary(OutSink& o) {
  if (cfg.cpuMhz != 0) o.println(trf("Governor:     fixed (%u MHz fissi)", (unsigned)cfg.cpuMhz));
  else o.println(trf("Governor:     %s", governorShown()));
  o.println(trf("Frequenza:    %u MHz   Tetto: %u MHz", (unsigned)drvCpuMhz(), (unsigned)cpuPmLimitMhz()));
  o.println(trf("Motivo:       %s", cpuPmWhyText(cpuPmWhy())));
  char t[64];
  tempLine(t, sizeof t);
  o.println(trf("Temperatura:  %s", t));
  o.println(trf("Carico:       core 0 %d%%, core 1 %d%%", sysCpuCorePercent(0), sysCpuCorePercent(1)));
}

// ---- cpu why ----
void cpuWhyLines(OutSink& o) {
  o.println(trf("Frequenza:    %u MHz   Tetto: %u MHz", (unsigned)drvCpuMhz(), (unsigned)cpuPmLimitMhz()));
  o.println(trf("Motivo:       %s", cpuPmWhyText(cpuPmWhy())));
  o.println(trf("Parametri:    salita %d%%, discesa %d%%, attesa %d s", (int)cfg.cpuUp, (int)cfg.cpuDown, (int)cfg.cpuHold));
  o.println(trf("Cambi di tetto dall'avvio: %lu", (unsigned long)cpuPmChanges()));
  if (cpuBoostActive()) o.println(trf("Prenotazione attiva: %s", cpuBoostWho()));
}

// ---- cpu info [-v] (come lscpu) ----
void cpuInfo(OutSink& o, bool verbose) {
  unsigned rev = (unsigned)drvChipRevision();
  o.println(trf("Chip:          %s rev %u.%u, %u core", drvChipModel(), rev / 100, rev % 100, (unsigned)drvChipCores()));
  o.println(tr("Frequenze:    80 160 240 MHz"));
  o.println(tr("Governor:     fixed ondemand conservative schedutil"));
  o.println(trf("Attuale:      %u MHz (tetto %u, scelta %s)", (unsigned)drvCpuMhz(), (unsigned)cpuPmLimitMhz(), cpuChoiceName()));
  if (drvPsramTotal()) o.println(trf("Memoria:      PSRAM %u KB, flash %u KB", (unsigned)(drvPsramTotal() / 1024), (unsigned)(drvFlashSize() / 1024)));
  else o.println(trf("Memoria:      PSRAM assente, flash %u KB", (unsigned)(drvFlashSize() / 1024)));
  o.println(trf("ESP-IDF:       %s", drvSdkVersion()));
  o.println(drvCpuPmReady() ? tr("Gestore energia: attivo") : tr("Gestore energia: non disponibile in questa build"));
  char t[64];
  tempLine(t, sizeof t);
  o.println(trf("Temperatura:  %s", t));
  if (!verbose) return;
  float warn, reduce, extreme, back;
  cpuPmThresholds(warn, reduce, extreme, back);
  o.println(trf("Parametri:    salita %d%%, discesa %d%%, attesa %d s", (int)cfg.cpuUp, (int)cfg.cpuDown, (int)cfg.cpuHold));
  o.println(trf("Soglie caldo: avviso %d C, riduzione %d C, estremo %d C, rientro sotto %d C", (int)(warn + 0.5f), (int)(reduce + 0.5f), (int)(extreme + 0.5f), (int)(back + 0.5f)));
  o.println(trf("Cambi di tetto dall'avvio: %lu", (unsigned long)cpuPmChanges()));
}

// ---- cpu governor [nome] ----
void cpuGovernor(const char* line, OutSink& o, int role) {
  Word a2 = argAt(line, 2);
  a2.lower();
  if (a2.empty()) {
    o.println(trf("In uso: %s   Disponibili: fixed ondemand conservative schedutil", governorShown()));
    return;
  }
  if (role != ROLE_ADMIN) { o.println(tr("Serve il livello Admin")); return; }
  if (a2 == "fixed" || a2.startsWith("fixed:")) {                       // "fixed 160" oppure "fixed:160"
    Word mhz = (a2 == "fixed") ? argAt(line, 3) : Word(a2.c_str() + 6, a2.length() - 6);
    if ((mhz == "80" || mhz == "160" || mhz == "240") && cpuChoiceSet(mhz.c_str())) {
      o.println(trf("Governor: fixed (%u MHz fissi)", (unsigned)cfg.cpuMhz));
      return;
    }
    o.println(tr("Uso: cpu governor fixed <80|160|240>"));
    return;
  }
  if (a2 == "auto") a2 = Word("ondemand", 8);                           // vecchio nome di ondemand
  CpuGov g = GOV_ONDEMAND;
  if (!cpuGovParse(a2.c_str(), g) || !cpuChoiceSet(a2.c_str())) {
    o.println(tr("Uso: cpu governor [ondemand|conservative|schedutil|fixed <80|160|240>]"));
    return;
  }
  o.println(trf("Governor: %s", governorShown()));
}

// ---- cpu freq [80|160|240] ----
void cpuFreq(const char* line, OutSink& o, int role) {
  Word a2 = argAt(line, 2);
  if (a2.empty()) {
    o.println(trf("Frequenza:    %u MHz   Tetto: %u MHz   Ammesse: 80 160 240 MHz", (unsigned)drvCpuMhz(), (unsigned)cpuPmLimitMhz()));
    return;
  }
  if (role != ROLE_ADMIN) { o.println(tr("Serve il livello Admin")); return; }
  if (!(a2 == "80" || a2 == "160" || a2 == "240") || !cpuChoiceSet(a2.c_str())) { o.println(tr("Uso: cpu freq [80|160|240]")); return; }
  o.println(trf("Velocita CPU: %u MHz (fissa)", (unsigned)cfg.cpuMhz));
}

// ---- cpu set up|down|hold <n> ----
void cpuSetParam(const char* line, OutSink& o) {
  Word name = argAt(line, 2), val = argAt(line, 3);
  ErrBuf err;
  if (name.empty() || val.empty()) { o.println(tr("Uso: cpu set up|down|hold <numero>")); return; }
  if (!cpuParamSet(name.c_str(), val.toInt(), err)) { o.println(err.c_str()); return; }
  o.println(trf("Parametro %s = %ld", name.c_str(), (long)val.toInt()));
}

}  // namespace

bool shCmdCpu(const Word& c, const Word& a1, const char* line, OutSink& o, int role) {
  if (c == "lscpu") { cpuInfo(o, a1 == "-v"); return true; }
  if (c == "vesevos-cpuinfo") {
    cpuSummary(o);
    o.println();
    cpuInfo(o, a1 == "-v");
    return true;
  }
  if (c != "cpu") return false;
  if (a1.empty()) cpuSummary(o);
  else if (a1 == "why") cpuWhyLines(o);
  else if (a1 == "info") cpuInfo(o, argAt(line, 2) == "-v");
  else if (a1 == "governor") cpuGovernor(line, o, role);
  else if (a1 == "freq") cpuFreq(line, o, role);
  else if (role != ROLE_ADMIN) o.println(tr("Serve il livello Admin"));      // da qui in poi si cambia qualcosa
  else if (a1 == "set") cpuSetParam(line, o);
  else if (cpuChoiceSet(a1.c_str())) o.println(trf("Velocita CPU: %s", cpuChoiceName()));   // vecchie forme: 80 160 240 ondemand conservative schedutil auto
  else o.println(tr("Uso: cpu [info [-v]|governor [ondemand|conservative|schedutil|fixed <80|160|240>]|freq [80|160|240]|why|set up|down|hold <numero>]"));
  return true;
}
