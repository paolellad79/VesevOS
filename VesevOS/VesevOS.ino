// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS v1.3.4 - ESP32-S3 SuperMini
// Fase 1 + 2: base, sicurezza, ora/NTP, file. Progetto diviso in piu file (.h/.cpp nella stessa cartella).
//
// Impostazioni Arduino IDE consigliate:
//   Scheda: ESP32S3 Dev Module
//   PSRAM: QSPI PSRAM
//   USB CDC On Boot: Enabled
//   Partition Scheme: Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)
// Librerie: ESP32Async ESPAsyncWebServer + AsyncTCP
#include <Arduino.h>
#include <LittleFS.h>
#include "vos_common.h"
#include "vos_log.h"
#include "vos_pins.h"
#include "vos_config.h"
#include "vos_auth.h"
#include "vos_sys.h"
#include "vos_led.h"
#include "vos_net.h"
#include "vos_time.h"
#include "vos_web.h"
#include "vos_shell.h"
#include "vos_i18n.h"

static void resetPasswordIfBootHeld() {
  // Tieni premuto BOOT (GPIO0) per 8 secondi a sistema acceso: azzera la password.
  static uint32_t since = 0;
  if (digitalRead(VOS_PIN_BOOT) == LOW) {
    if (since == 0) since = millis();
    else if (millis() - since > 8000) {
      vlog("AUTH: reset password da pulsante BOOT");
      cfg.authSalt = ""; cfg.authHash = ""; cfg.serialAuth = true;
      cfgSave();
      serialAuthSet(false);
      since = 0;
      ledSetFault(true); delay(1500); ledSetFault(false);
    }
  } else since = 0;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  logInit();
  vlog("%s %s in avvio", VOS_NAME, VOS_VERSION);
  pinsInit();
  pinMode(VOS_PIN_BOOT, INPUT_PULLUP);
  pinClaim(VOS_PIN_BOOT, "Sistema", "Pulsante BOOT (8 s = reset password)", true);

  if (!LittleFS.begin(true)) { vlog("FS: LittleFS non parte"); }
  if (!cfgLoad()) { vlog("CFG: nessun file, uso i valori iniziali"); cfgSave(); }

  langInit();
  sysInit();
  ledInit();
  netInit();
  timeInit();
  webInit();
  vlog("Pronto. Primo accesso: rete '%s' -> http://192.168.4.1", cfg.apSsid.c_str());
}

void loop() {
  shellSerialPoll();
  resetPasswordIfBootHeld();
  pinTestTick();
  delay(10);
}
