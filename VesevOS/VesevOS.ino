// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS 1.7.65a - ESP32-S3 SuperMini
// Piccolo sistema operativo: pagina web (HTTPS), shell, utenti, rete tra schede, automazioni. Progetto in piu file.
//
// Impostazioni Arduino IDE consigliate:
//   Scheda: ESP32S3 Dev Module
//   PSRAM: QSPI PSRAM
//   USB CDC On Boot: Enabled
//   Partition Scheme: Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)
// Librerie da installare (Gestore librerie): PsychicHttp (hoeken, MIT) e ArduinoJson (bblanchon, MIT).
// ESPAsyncWebServer e AsyncTCP non servono piu dalla 1.7.1.
#include <Arduino.h>
#include "src/drivers/vos_drv_fs.h"
#include "src/vos_wire.h"
#include <esp_log.h>
#include "src/core/vos_ram.h"
#include "src/core/vos_common.h"
#include "src/core/vos_log.h"
#include "src/core/vos_diario.h"
#include "src/core/vos_crash.h"
#include "src/core/vos_serial.h"
#include "src/devices/vos_pins.h"
#include "src/devices/vos_io.h"
#include "src/core/vos_config.h"
#include "src/security/vos_auth.h"
#include "src/packages/vos_mfa.h"
#include "src/packages/vos_power.h"
#include "src/packages/vos_stats.h"
#include "src/core/vos_sys.h"
#include "src/devices/vos_led.h"
#include "src/net/vos_net.h"
#include "src/packages/vos_btn.h"
#include "src/core/vos_time.h"
#include "src/interfaces/vos_web.h"
#include "src/interfaces/vos_shell.h"
#include "src/core/vos_i18n.h"
#include "src/core/vos_boot.h"
#include "src/core/vos_audit.h"
#include "src/core/vos_fw.h"
#include "src/core/vos_wd.h"
#include "src/packages/vos_ble.h"
#include "src/drivers/vos_drv_gpio.h"

// Tasto BOOT (GPIO0). L'azione si decide quando lo LASCI; intanto il LED dice cosa succedera:
//   meno di 2 s (click)    : 1, 2 o 3 click veloci fanno l'azione scelta nella pagina (di fabbrica: 1 = esce dal modo aereo, 2 = WPS)
//   2-7 s   (LED azzurro)  : spegne il filtro IP (uscita di emergenza)
//   8-19 s  (LED giallo)   : azzera le password degli amministratori + mostra sulla seriale la password dell'hotspot
//   20 s o piu (LED rosso) : ripristino di fabbrica (cancella tutto) e riavvio
static void bootButton() {
  static uint32_t since = 0;
  if (!drvGpioRead(VOS_PIN_BOOT)) {
    if (since == 0) since = millis();
    uint32_t held = millis() - since;
    ledSetHold(held >= 20000 ? 3 : held >= 8000 ? 2 : held >= 2000 ? 1 : 0);
    return;
  }
  if (!since) return;
  uint32_t held = millis() - since;
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
    ledSetFault(true); delay(1500); ledSetFault(false);
    if (svc) sysReboot("tasto BOOT 8 s", 300);   // porte e server si applicano al riavvio
  } else if (held >= 2000) {
    fwOff("tasto BOOT");
  } else if (held >= 50) {
    btnClick();
  }
}

// I blocchi grandi (buffer TLS da 16 KB, ecc.) vanno in PSRAM e lasciano libera la RAM interna.
// Funzione del sistema: se il tuo core non la contiene il programma parte lo stesso (nessun errore di compilazione).
extern "C" void heap_caps_malloc_extmem_enable(size_t limit) __attribute__((weak));
// Marchio che il recovery cerca nel file prima di installarlo (non cambiare: vedi recovery/VesevOS_Recovery/recovery_check.h)
static const char VOS_FW_MARK[] __attribute__((used)) = "{FW:VesevOS}";
bool g_extmem = false;                                // letto da shell (comando diag)

void setup() {
  (void)*(volatile const char*)VOS_FW_MARK;          // tiene il marchio nel file anche dopo l'ottimizzazione
  vosWire();                                        // collega pagina e package ai ganci del nucleo (prima di ogni avvio)
  diaryEarly();                                     // prima di tutto: salva l'istantanea lasciata dall'avvio precedente
  crashEarly();                                     // rapporto dell'eventuale crash precedente (memoria RTC)
  crashInstall();                                   // da qui un crash lascia motivo e backtrace
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(10);                       // attesa massima 10 ms a scrittura (era 100): se il PC non legge, loop non si ferma per secondi; con 0 si perdevano pezzi di testo
#endif
  delay(300);
  logInit();
  if (VOS_PSRAM_MALLOC && psramFound() && heap_caps_malloc_extmem_enable) { heap_caps_malloc_extmem_enable(256); g_extmem = true; ramTlsToPsram(); }
  esp_log_level_set("esp-tls-mbedtls", ESP_LOG_NONE);   // un browser che non si fida del certificato non riempie la seriale di errori ogni 30 s
  esp_log_level_set("esp_https_server", ESP_LOG_NONE);
  esp_log_level_set("httpd", ESP_LOG_NONE);
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
  serialApply();                                   // velocita, attesa di scrittura
  setenv("TZ", cfg.tz.c_str(), 1); tzset();         // il fuso subito: cosi le righe del registro hanno l'ora locale fin dall'inizio
  vlog("%s %s in avvio", VOS_NAME, VOS_VERSION);
  crashBoot();                                      // se il riavvio viene da un crash: log e /crash.txt

  diaryStage("io");
  ioInit();                                        // periferiche a classi (rele, led, pin...): uscite spente
  diaryStage("lang");
  langInit();
  auditInit();                                     // controllo della configurazione (allarmi)
  bleServiceInit();                                // servizi iscritti nel registro (vos_service) prima dell'avvio
  bootRun();   // sys, led, net, time, web, rules, mqtt, mesh, wd: nell'ordine scelto (con le dipendenze)
  diaryStage("avviato");
  ramNote("avviato");
  diaryInit();                                     // dopo sysInit: contatore avvii pronto; scrive la riga AVVIO e, se anomalo, i dettagli
  ledSetSetup(!cfg.setupDone);                     // arcobaleno lento finche la prima configurazione non e finita
  vlog("Pronto. Guida di configurazione: %s", cfg.setupDone ? "finita" : "NON finita (hotspot e HTTP forzati accesi)");   // la rete la dicono le righe NET: seguenti
}

void loop() {
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
  diaryStage("L:wdt");    feedLoopWDT();
  delay(10);
}
