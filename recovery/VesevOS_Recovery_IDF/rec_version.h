// VesevOS Recovery - versione e marchio.
// Il formato del marchio "{REC:VesevOS:" + versione + "}" NON si cambia: la pagina di VesevOS 1.7.58+ lo cerca
// (vedi VesevOS/src/core/vos_imgcheck.h) per aggiornare il recovery e leggerne la versione.
#pragma once

#define REC_VERSION "0.5.4f"

// Tempi (millisecondi), uguali alla 0.4.4.
#define HANDOVER_MS 3000
#define STA_WAIT_MS 30000UL
#define IDLE_MS     900000UL     // 15 minuti senza accessi: se VesevOS e valido, ci torna da solo
#define TOKEN_MS    600000UL
#define NONCE_MS    60000UL
#define HASH_ITER   3000

// Pin del tasto BOOT (S3: GPIO0; C6 e C3 SuperMini: GPIO9, da verificare con lo schema).
// REC_LED_PIN e il pin del LED RGB WS2812; -1 = la scheda non ne ha (C3: un solo LED blu semplice su GPIO8), il recovery non pilota nessun LED.
#if __has_include(<sdkconfig.h>)
#include <sdkconfig.h>
#endif
#ifdef CONFIG_IDF_TARGET_ESP32C6
#define BOOT_PIN 9
#define REC_LED_PIN 8
#define REC_CHIP_ID 0x000D
#define REC_CHIP_NAME "ESP32-C6"
#define REC_SCHEDA "ESP32-C6 SuperMini"   // nome della scheda, mostrato nella pagina (come VOS_BOARD del firmware)
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define BOOT_PIN 9
#define REC_LED_PIN -1
#define REC_CHIP_ID 0x0005
#define REC_CHIP_NAME "ESP32-C3"
#define REC_SCHEDA "ESP32-C3 SuperMini"
#else
#define BOOT_PIN 0
#define REC_LED_PIN 48
#define REC_CHIP_ID 0x0009
#define REC_CHIP_NAME "ESP32-S3"
#define REC_SCHEDA "ESP32-S3 SuperMini"
#endif
