// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS 1.8.0b - ESP32-S3 SuperMini
// Piccolo sistema operativo: pagina web (HTTPS), shell, utenti, rete tra schede, automazioni. Progetto in piu file.
//
// vos_main.cpp: punto di ingresso del firmware (app_main di ESP-IDF). Prima stava nello sketch Arduino (setup e loop).
// Si compila con ESP-IDF 5.5 (progetto idf/, fuori dal repo). Qui si collega tutto, come in vos_wire.cpp: livello "8" per checklayers.
//
// TASK PRINCIPALE (era il loopTask di Arduino, stessi valori di fabbrica del core Arduino-ESP32 3.3):
//   nome "loopTask" (resta quello vecchio: lo cercano autodiagnosi, tabella degli stack e rapporti dei crash),
//   core AUTOMATICO (prima: core 1 fisso; vedi drivers/vos_drv_task.h), priorita 1 (la piu bassa dopo l'idle), stack 8192 byte in RAM interna.
//   Fa prima vosSetup() (una volta), poi vosLoop() senza fine. Il task e iscritto al watchdog dei task (30 s di base, panico + riavvio)
//   da wdInit() con drvLoopWdtEnable(); a ogni giro vosLoop() lo alimenta. Il task "main" di ESP-IDF crea il task principale e finisce.
#include <stdlib.h>
#include "drivers/vos_drv_task.h"
#include <time.h>
#include "vos_wire.h"
#include "drivers/vos_drv_fs.h"
#include "core/vos_ram.h"
#include "core/vos_common.h"
#include "core/vos_log.h"
#include "core/vos_tmpclean.h"
#include "core/vos_diario.h"
#include "core/vos_crash.h"
#include "core/vos_serial.h"
#include "devices/vos_pins.h"
#include "devices/vos_io.h"
#include "core/vos_config.h"
#include "security/vos_auth.h"
#include "packages/vos_mfa.h"
#include "packages/vos_power.h"
#include "packages/vos_stats.h"
#include "core/vos_sys.h"
#include "devices/vos_led.h"
#include "net/vos_net.h"
#include "packages/vos_btn.h"
#include "core/vos_time.h"
#include "interfaces/vos_web.h"
#include "interfaces/vos_shell.h"
#include "core/vos_i18n.h"
#include "core/vos_boot.h"
#include "core/vos_audit.h"
#include "core/vos_updorder.h"
#include "core/vos_fw.h"
#include "core/vos_wd.h"
#include "core/vos_wdset.h"
#include "packages/vos_ble.h"
#include "drivers/vos_drv_gpio.h"
#include "drivers/vos_drv_time.h"
#include "drivers/vos_drv_console.h"
#include "drivers/vos_drv_boot.h"
#include "drivers/vos_drv_heap.h"
#include "drivers/vos_drv_sys.h"

// Task principale: valori scelti per fare come il core Arduino (vedi la nota in cima al file).
static const char*    MAIN_TASK_NAME  = "loopTask";
static const uint32_t MAIN_TASK_STACK = 8192;     // byte
static const int      MAIN_TASK_PRIO  = 1;
static const int      MAIN_TASK_CORE  = DRV_CORE_ANY;   // core: automatico (prima: core 1 fisso)

// Tasto BOOT (GPIO0). L'azione si decide quando lo LASCI; intanto il LED dice cosa succedera:
//   meno di 2 s (click)    : 1, 2 o 3 click veloci fanno l'azione scelta nella pagina (di fabbrica: 1 = esce dal modo aereo, 2 = WPS)
//   2-7 s   (LED azzurro)  : spegne il filtro IP (uscita di emergenza)
//   8-19 s  (LED giallo)   : azzera le password degli amministratori + mostra sulla seriale la password dell'hotspot
//   20 s o piu (LED rosso) : ripristino di fabbrica (cancella tutto) e riavvio
static void bootButton() {
  static uint32_t since = 0;
  if (!drvGpioRead(VOS_PIN_BOOT)) {
    if (since == 0) since = drvMillis();
    uint32_t held = drvMillis() - since;
    ledSetHold(held >= 20000 ? 3 : held >= 8000 ? 2 : held >= 2000 ? 1 : 0);
    return;
  }
  if (!since) return;
  uint32_t held = drvMillis() - since;
  since = 0;
  ledSetHold(0);
  if (held >= 2000) btnLongRelease();
  if (btnTestActive()) { if (held >= 2000) btnTestLong(held); else if (held >= 50) btnClick(); return; }   // modo prova: niente azioni
  cfgSetOrigin("tasto BOOT");
  statNote(held >= 20000 ? ST_BOOT_20S : held >= 8000 ? ST_BOOT_8S : held >= 2000 ? ST_BOOT_2S : ST_BOOT_SHORT);
  if (held >= 20000) {
    vlog("SISTEMA: ripristino di fabbrica con il tasto BOOT");
    cfgFactoryReset(); sysReboot("ripristino di fabbrica con il tasto BOOT", 300);
  } else if (held >= 8000) {
    authResetAdmin();
    int mf = mfaOffAdmins();
    if (mf) vlog("SICUREZZA: MFA disattivato per %d amministratori con il tasto BOOT", mf);
    cfg.serialAuth = true;
    bool svc = cfgSvcRecover();       // hotspot, HTTP, HTTPS e porte standard: si rientra sempre
    cfgSave();
    webAllowFirstPass();
    shellSerialShowPass();
    ledSetFault(true); drvDelayMs(1500); ledSetFault(false);
    if (svc) sysReboot("tasto BOOT 8 s", 300);   // porte e server si applicano al riavvio
  } else if (held >= 2000) {
    fwOff("tasto BOOT");
  } else if (held >= 50) {
    btnClick();
  }
}

// Marchio che il recovery cerca nel file prima di installarlo (non cambiare: vedi recovery/VesevOS_Recovery_IDF/recovery_check.h)
static const char VOS_FW_MARK[] __attribute__((used)) = "{FW:VesevOS}";
bool g_extmem = false;                                // letto da shell (comando diag): i blocchi grandi vanno in PSRAM?
static bool g_nvsOk = true;                           // drvPlatformInit() ha avviato la NVS?

static void vosSetup() {
  (void)*(volatile const char*)VOS_FW_MARK;          // tiene il marchio nel file anche dopo l'ottimizzazione
  vosWire();                                        // collega pagina e package ai ganci del nucleo (prima di ogni avvio)
  diaryEarly();                                     // prima di tutto: salva l'istantanea lasciata dall'avvio precedente
  crashEarly();                                     // rapporto dell'eventuale crash precedente (memoria RTC)
  crashInstall();                                   // da qui un crash lascia motivo e backtrace
  drvConBegin(115200);
  drvConSetTxTimeoutMs(10);                       // attesa massima 10 ms a scrittura (era 100): se il PC non legge, loop non si ferma per secondi; con 0 si perdevano pezzi di testo
  drvDelayMs(300);
  logInit();
  if (!g_nvsOk) vlog("NVS: non parte (memoria dei dati di sistema)");
  if (VOS_PSRAM_MALLOC && drvPsramTotal() > 0) { drvExtmemEnable(256); g_extmem = true; ramTlsToPsram(); }
  drvLogOff("esp-tls-mbedtls");                     // un browser che non si fida del certificato non riempie la seriale di errori ogni 30 s
  drvLogOff("esp_https_server");
  drvLogOff("httpd");
  diaryStage("pins");
  pinsInit();
  drvGpioMode(VOS_PIN_BOOT, DG_INPUT_PULLUP);
  pinClaim(VOS_PIN_BOOT, "Sistema", "Pulsante BOOT (click = azioni scelte, 2 s = filtro IP, 8 s = password, 20 s = fabbrica)", true);

  diaryStage("fs");
  if (!drvFsBegin(true)) { vlog("FS: LittleFS non parte"); }
  powerBegin();
  diaryStage("cfg");
  if (!cfgLoad()) { vlog("CFG: nessun file, uso i valori iniziali"); cfgSave(); }
  logSetLevel(cfg.logLevel);
  wdSetInit();                                     // tempo salvato del Task watchdog (gia dopo la lettura della config)
  serialApply();                                   // velocita, attesa di scrittura
  setenv("TZ", cfg.tz.c_str(), 1); tzset();         // il fuso subito: cosi le righe del registro hanno l'ora locale fin dall'inizio
  vlog("%s %s in avvio", VOS_NAME, VOS_VERSION);
  crashBoot();                                      // se il riavvio viene da un crash: log e /crash.txt

  diaryStage("io");
  ioInit();                                        // periferiche a classi (rele, led, pin...): uscite spente
  diaryStage("lang");
  langInit();
  // La PSRAM parte presto (SPIRAM_BOOT_INIT); qui si dice solo se c'e: senza avviso un guasto passerebbe inosservato.
  if (drvPsramTotal() > 0) vlog("%s", trf("PSRAM: %lu KB", (unsigned long)(drvPsramTotal() / 1024)).c_str());
  else vlog("%s", tr("PSRAM non trovata: uso solo la RAM interna"));
  auditInit();                                     // controllo della configurazione (allarmi)
  updOutcomeBoot();                                // esito di un aggiornamento fatto dal recovery (registro e audit), poi lo cancella
  bleServiceInit();                                // servizi iscritti nel registro (vos_service) prima dell'avvio
  bootRun();   // sys, led, net, time, web, rules, mqtt, mesh, wd: nell'ordine scelto (con le dipendenze)
  diaryStage("avviato");
  ramNote("avviato");
  tmpCleanStart();                                 // tra 30 s toglie i .tmp~ lasciati da un taglio di corrente (task di fondo, una volta)
  diaryInit();                                     // dopo sysInit: contatore avvii pronto; scrive la riga AVVIO e, se anomalo, i dettagli
  ledSetSetup(!cfg.setupDone);                     // arcobaleno lento finche la prima configurazione non e finita
  vlog("Pronto. Guida di configurazione: %s", cfg.setupDone ? "finita" : "NON finita (hotspot e HTTP forzati accesi)");   // la rete la dicono le righe NET: seguenti
}

static void vosLoop() {
  static bool first = true; if (first) { first = false; diaryStage("L:start"); }
  diaryStage("L:shell");  shellSerialPoll();
  diaryStage("L:button"); bootButton(); btnTick(!drvGpioRead(VOS_PIN_BOOT));
  diaryStage("L:ble");    bleTick();
  diaryStage("L:power");  powerTick();
  diaryStage("L:stats");  statsTick();
  diaryStage("L:pin");    pinTestTick();
  diaryStage("L:io");     ioTick();
  diaryStage("L:stable"); bootStable();
  diaryStage("L:serial"); serialTick();
  diaryStage("L:wdt");    drvLoopWdtFeed();
  drvDelayMs(10);
}

// Il task principale: l'avvio una volta, poi il ciclo. Il watchdog si alimenta anche prima di ogni giro, come faceva il loopTask di Arduino
// (cosi il primo giro dopo un avvio lungo parte con il tempo intero).
static void mainTask(void*) {
  vosSetup();
  for (;;) {
    drvLoopWdtFeed();
    vosLoop();
  }
}

// Ingresso di ESP-IDF (al posto di setup/loop di Arduino). Prepara la piattaforma e lancia il task principale.
extern "C" void app_main(void) {
  g_nvsOk = drvPlatformInit();                      // log di fabbrica e NVS: prima di tutto, come faceva initArduino
  if (!drvMainTaskStart(mainTask, MAIN_TASK_NAME, MAIN_TASK_STACK, MAIN_TASK_PRIO, MAIN_TASK_CORE)) drvSysRestart();   // niente memoria per il task: riprova col riavvio
}
