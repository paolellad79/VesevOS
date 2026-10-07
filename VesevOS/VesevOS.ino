// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS 1.7.5 - ESP32-S3 SuperMini
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
#include <LittleFS.h>
#include <esp_log.h>
#include "vos_common.h"
#include "vos_log.h"
#include "vos_pins.h"
#include "vos_config.h"
#include "vos_auth.h"
#include "vos_mfa.h"
#include "vos_power.h"
#include "vos_stats.h"
#include "vos_sys.h"
#include "vos_led.h"
#include "vos_net.h"
#include "vos_time.h"
#include "vos_web.h"
#include "vos_shell.h"
#include "vos_i18n.h"
#include "vos_boot.h"
#include "vos_audit.h"
#include "vos_fw.h"
#include "vos_ble.h"

// Tasto BOOT (GPIO0). L'azione si decide quando lo LASCI; intanto il LED dice cosa succedera:
//   meno di 2 s           : esce dal modo aereo
//   2-7 s   (LED azzurro)  : spegne il filtro IP (uscita di emergenza)
//   8-19 s  (LED giallo)   : azzera le password degli amministratori + mostra sulla seriale la password dell'hotspot
//   20 s o piu (LED rosso) : ripristino di fabbrica (cancella tutto) e riavvio
static void bootButton() {
  static uint32_t since = 0;
  if (digitalRead(VOS_PIN_BOOT) == LOW) {
    if (since == 0) since = millis();
    uint32_t held = millis() - since;
    ledSetHold(held >= 20000 ? 3 : held >= 8000 ? 2 : held >= 2000 ? 1 : 0);
    return;
  }
  if (!since) return;
  uint32_t held = millis() - since;
  since = 0;
  ledSetHold(0);
  cfgSetOrigin("tasto BOOT");
  statNote(held >= 20000 ? ST_BOOT_20S : held >= 8000 ? ST_BOOT_8S : held >= 2000 ? ST_BOOT_2S : ST_BOOT_SHORT);
  if (held >= 20000) {
    vlog("SISTEMA: ripristino di fabbrica con il tasto BOOT");
    cfgFactoryReset(); delay(300); ESP.restart();
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
    if (svc) { delay(300); ESP.restart(); }   // porte e server si applicano al riavvio
  } else if (held >= 2000) {
    fwOff("tasto BOOT");
  } else if (held >= 50 && netAirplane()) {
    netAirplaneOff("tasto BOOT");
  }
}

// I blocchi grandi (buffer TLS da 16 KB, ecc.) vanno in PSRAM e lasciano libera la RAM interna.
// Funzione del sistema: se il tuo core non la contiene il programma parte lo stesso (nessun errore di compilazione).
extern "C" void heap_caps_malloc_extmem_enable(size_t limit) __attribute__((weak));
bool g_extmem = false;                                // letto da shell (comando diag)

void setup() {
  Serial.begin(115200);
  delay(300);
  logInit();
  if (VOS_PSRAM_MALLOC && psramFound() && heap_caps_malloc_extmem_enable) { heap_caps_malloc_extmem_enable(4096); g_extmem = true; }
  esp_log_level_set("esp-tls-mbedtls", ESP_LOG_NONE);   // un browser che non si fida del certificato non riempie la seriale di errori ogni 30 s
  esp_log_level_set("esp_https_server", ESP_LOG_NONE);
  esp_log_level_set("httpd", ESP_LOG_NONE);
  pinsInit();
  pinMode(VOS_PIN_BOOT, INPUT_PULLUP);
  pinClaim(VOS_PIN_BOOT, "Sistema", "Pulsante BOOT (breve = modo aereo, 2 s = filtro IP, 8 s = password, 20 s = fabbrica)", true);

  if (!LittleFS.begin(true)) { vlog("FS: LittleFS non parte"); }
  powerBegin();
  if (!cfgLoad()) { vlog("CFG: nessun file, uso i valori iniziali"); cfgSave(); }
  setenv("TZ", cfg.tz.c_str(), 1); tzset();         // il fuso subito: cosi le righe del registro hanno l'ora locale fin dall'inizio
  vlog("%s %s in avvio", VOS_NAME, VOS_VERSION);

  langInit();
  auditInit();                                     // controllo della configurazione (allarmi)
  bootRun();   // sys, led, net, time, web, rules, mqtt, mesh, wd: nell'ordine scelto (con le dipendenze)
  ledSetSetup(!cfg.setupDone);                     // arcobaleno lento finche la prima configurazione non e finita
  vlog("Pronto. Guida di configurazione: %s", cfg.setupDone ? "finita" : "NON finita (hotspot e HTTP forzati accesi)");   // la rete la dicono le righe NET: seguenti
}

void loop() {
  shellSerialPoll();
  bootButton();
  bleTick();
  powerTick();
  statsTick();
  pinTestTick();
  bootStable();
  feedLoopWDT();
  delay(10);
}
