// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - boards/vos_board_h2_supermini.h
// File di scheda: ESP32-H2 SuperMini (RISC-V 96 MHz, 4 MB flash nel package, niente PSRAM, NIENTE Wi-Fi: Bluetooth LE e 802.15.4,
// USB seriale/JTAG, LED WS2812 su GPIO8). Preliminare (porting H2 non iniziato, mai compilato con il firmware completo):
// i pin vengono da fonte di terza parte (espboards.dev, riassunta in .claude/skills/schede-e-hardware/fonti/) e dal datasheet del chip;
// dove c'e scritto "da verificare" il valore e prudente e va confermato con lo schema della scheda. Solo caratteri ASCII.
// Verificato sulla scheda (19/11/2026): ESP32-H2 rev. v0.1, flash 4 MB, cristallo 32 MHz, LED WS2812 su GPIO8 con ordine RGB (a vista).
// Il modello per le voci e vos_board_c6_supermini.h; qui c'e solo cio che cambia.
#pragma once

#define VOS_BOARD         "ESP32-H2 SuperMini"
#define VOS_CHIP          "ESP32-H2"
#define VOS_CPU_SIGLA     "H2"                   // sigla nei nomi dei file e in aggiorna/latest.json (blocco "schede")
#define VOS_ADDR2LINE     "riscv32-esp-elf-addr2line"

// ---- memoria ----
#define VOS_FLASH_MB      4
#define VOS_PSRAM_MB      0
#define VOS_SRAM_KB       320           // SRAM interna del chip

// ---- antenna (dato dell'utente, 16/11/2026: CrossAir CA-C03 su tutte le SuperMini) ----
// Guadagno di picco in decimi di dBi (43 = 4,3 dBi). Serve all'EIRP del Bluetooth/802.15.4 (core/vos_region_calc.h).
#define VOS_ANTENNA_DECIMI_DBI  43

// ---- frequenza della CPU (limite e gradini per esp_pm) ----
// H2: massimo 96 MHz; ESP-IDF 5.5.5 offre 48, 64 e 96 MHz (skill chip-esp32-h2, fonti/frequenze-cpu.md). Gradini sotto i 48: non verificati.
#define VOS_CPU_MHZ_MIN    48
#define VOS_CPU_MHZ_MID    64
#define VOS_CPU_MHZ_MAX    96

// ---- cosa c'e sulla scheda ----
#define VOS_HAS_PSRAM        0
#define VOS_HAS_USB_NATIVE   1          // USB seriale/JTAG integrata
#define VOS_HAS_BLE          1
#define VOS_HAS_WIFI         0          // l'H2 NON ha il Wi-Fi: rete, pagina web e aggiornamenti dalla rete richiedono il profilo "senza Wi-Fi" (da sviluppare)
#define VOS_HAS_TEMP_SENSOR  1

// ---- LED e tasto ----
#define VOS_PIN_LED_RGB    8            // LED WS2812 integrato (strapping: lo usa anche l'avvio)
#define VOS_LED_TYPE      "WS2812"
#define VOS_LED_ORDER      VOS_LED_ORD_RGB   // verificato a vista dall'utente il 19/11/2026 (rosso, verde, blu); gli altri colori non provati
// Secondo LED: blu utente su GPIO13 (fonte di terza parte). Livello attivo DA VERIFICARE con lo schema.
#define VOS_PIN_LED2      13
#define VOS_LED2_ACTIVE_LOW 0           // 0 = acceso a livello alto: DA VERIFICARE
#define VOS_LED_DIAG_PANICO 1
#define VOS_PIN_BOOT       9            // tasto BOOT (strapping)

// ---- periferiche di bordo ----
#define VOS_BOARD_PERIF(X) \
  X("led_rgb1", "LED_RGB1",     "",     "Luce",     "led_rgb1", LEDRGB,   "colore", VOS_PIN_LED_RGB, "LED RGB della scheda") \
  X("led1",     "LED1",         "",     "Luce",     "led1",     LEDONOFF, "onoff",  VOS_PIN_LED2,    "LED della scheda") \
  X("boot",     "Tasto fisico", "",     "Pulsante", "btn",      TASTO,    "tasto",  VOS_PIN_BOOT,    "Tasto fisico")

// ---- GPIO ----
// Il chip ha 19 GPIO: 0-5, 8-14 e 22-27 (datasheet H2; riassunto TRM cap. 6). Su questa scheda 6, 7 e 15-21 non esistono o sono flash.
#define VOS_GPIO_MAX       27
#define VOS_GPIO_COUNT     28
#define VOS_GPIO_EXISTS(g) (((g) >= 0 && (g) <= 5) || ((g) >= 8 && (g) <= 14) || ((g) >= 22 && (g) <= 27))
#define VOS_PIN_USB_DM    26            // USB D-
#define VOS_PIN_USB_DP    27            // USB D+
// Pin di avvio: 8, 9, 25 (datasheet H2) e 2-5 (JTAG, prudenza): meglio non toccarli. Da riconfrontare con la tabella dei pin del datasheet.
#define VOS_GPIO_STRAP(g)  ((g) == 8 || (g) == 9 || (g) == 25 || ((g) >= 2 && (g) <= 5))

// ---- analogico e I2C ----
#define VOS_ADC_FIRST      1            // ADC1 dell'H2: GPIO1-5 (il canale 4 = GPIO5 non e disponibile: errata ADC-7227 sulla v0.1, da trattare)
#define VOS_ADC_LAST       5
#define VOS_I2C_SDA        10           // da verificare: pin liberi, diversi da LED (8, 13) e tasto (9)
#define VOS_I2C_SCL        11

// ---- microSD (nessun pin di fabbrica: modulo SPI esterno su pin liberi, da verificare) ----
#define VOS_STR_(x) #x
#define VOS_STR(x) VOS_STR_(x)
#define VOS_HAS_SD        1
#define VOS_SD_DEF_CS         12
#define VOS_SD_DEF_MOSI       14
#define VOS_SD_DEF_SCK        22
#define VOS_SD_DEF_MISO       1
#define VOS_SD_DEF_LIST   VOS_STR(VOS_SD_DEF_CS) "," VOS_STR(VOS_SD_DEF_MOSI) "," VOS_STR(VOS_SD_DEF_SCK) "," VOS_STR(VOS_SD_DEF_MISO)

// ---- avvisi mostrati dalla pagina (solo testo) ----
// L'H2 non ha ADC2 ne PSRAM: gli intervalli puntano a un pin che non esiste (28), cosi' non scattano mai.
#define VOS_ADC2_FIRST    28
#define VOS_ADC2_LAST     28
#define VOS_JTAG_FIRST     2            // JTAG esterno dell'H2: GPIO2-5 (secondo la fonte di terza parte; da verificare)
#define VOS_JTAG_LAST      5
#define VOS_PIN_UART_TX   24            // console seriale UART0
#define VOS_PIN_UART_RX   23
#define VOS_PSRAMQ_FIRST  28
#define VOS_PSRAMQ_LAST   28

// ---- PWM e contatore di impulsi (errata LEDC-253 e PCNT-249 sulla v0.1: da valutare prima di fidarsi) ----
#define VOS_BOARD_PWM 1
#define VOS_BOARD_CONTATORE 1
// Capacita del chip per il registratore di dati: l'H2 non ha coprocessore; la memoria a basso consumo e di 4 KB (da riverificare sul TRM H2 cap. 11).
#define VOS_HAS_ULP          0
#define VOS_HAS_LP_CPU       0
#define VOS_HAS_PCNT         1
#define VOS_RTC_MEM_BYTES    4096
// ---- LED a colori WS2812 a mano (classe "led_rgb"): canali RMT di trasmissione dell'H2 da verificare, 1 e del LED di bordo ----
#define VOS_BOARD_LED_RGB_MANO 1

// ---- moduli ELF: l'H2 e RISC-V come il C6; valori prudenti, da verificare ----
#define VOS_ELF_ARCH           2        // 1 = Xtensa, 2 = RISC-V
#define VOS_ELF_MACHINE        243      // RISC-V
#define VOS_ELF_ABI            0
#define VOS_ELF_HAS_PSRAM      0
#define VOS_ELF_EXEC_PSRAM     0
#define VOS_ELF_RAM_MAX_MODULE 32768    // 320 KB di SRAM e niente PSRAM: tetto piu basso del C6
#define VOS_ELF_FILE_MAX       65536

// ---- tabella dei piedini (tab Pin) ----
// Fonte: pinout della SuperMini di terza parte + datasheet H2. DA VERIFICARE A VISTA: ordine dei piedini lungo i lati e pad del retro.
//   Sinistra (dall'alto, lato USB): 24 23 0 1 2 3 4 5 8. Destra: 5V GND 3V3 14 13 12 11 10 9. Retro: 25 22 26 27 e BAT.
// Funzioni: ADC1 = GPIO1-5, SPI (FSPI) = 0-5, UART0 = 23/24, USB = 26/27, JTAG = 2-5. Flag RTC non messi (non verificati).
#define VOS_BOARD_PINS(X) \
  X(VOS_PL_SIN, 0, 24, VOS_PT_GPIO, VOS_PF_UART | VOS_PF_RISERVATO) \
  X(VOS_PL_SIN, 1, 23, VOS_PT_GPIO, VOS_PF_UART | VOS_PF_RISERVATO) \
  X(VOS_PL_SIN, 2,  0, VOS_PT_GPIO, VOS_PF_SPI) \
  X(VOS_PL_SIN, 3,  1, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_SPI) \
  X(VOS_PL_SIN, 4,  2, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_SPI | VOS_PF_JTAG | VOS_PF_AVVIO) \
  X(VOS_PL_SIN, 5,  3, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_SPI | VOS_PF_JTAG | VOS_PF_AVVIO) \
  X(VOS_PL_SIN, 6,  4, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_SPI | VOS_PF_JTAG | VOS_PF_AVVIO) \
  X(VOS_PL_SIN, 7,  5, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_SPI | VOS_PF_JTAG | VOS_PF_AVVIO) \
  X(VOS_PL_SIN, 8,  8, VOS_PT_GPIO, VOS_PF_AVVIO | VOS_PF_LED) \
  X(VOS_PL_DES, 0, -1, VOS_PT_5V, 0) \
  X(VOS_PL_DES, 1, -1, VOS_PT_GND, 0) \
  X(VOS_PL_DES, 2, -1, VOS_PT_3V3, 0) \
  X(VOS_PL_DES, 3, 14, VOS_PT_GPIO, 0) \
  X(VOS_PL_DES, 4, 13, VOS_PT_GPIO, VOS_PF_LED) \
  X(VOS_PL_DES, 5, 12, VOS_PT_GPIO, 0) \
  X(VOS_PL_DES, 6, 11, VOS_PT_GPIO, 0) \
  X(VOS_PL_DES, 7, 10, VOS_PT_GPIO, 0) \
  X(VOS_PL_DES, 8,  9, VOS_PT_GPIO, VOS_PF_AVVIO | VOS_PF_TASTO) \
  X(VOS_PL_RETRO_SIN, 0, 25, VOS_PT_GPIO, VOS_PF_AVVIO) \
  X(VOS_PL_RETRO_SIN, 1, 22, VOS_PT_GPIO, 0) \
  X(VOS_PL_RETRO_DES, 0, 26, VOS_PT_GPIO, VOS_PF_USB | VOS_PF_RISERVATO) \
  X(VOS_PL_RETRO_DES, 1, 27, VOS_PT_GPIO, VOS_PF_USB | VOS_PF_RISERVATO) \
  X(VOS_PL_RETRO_ALTO, 0, -1, VOS_PT_BATT_MENO, 0) \
  X(VOS_PL_RETRO_ALTO, 1, -1, VOS_PT_BATT_PIU, 0)

// ---- pin consigliati: solo i liberi sicuri (0, 1, 10, 11, 12, 14, 22), SDA e SCL in testa alla lista I2C ----
#define VOS_GPIO_SUGGEST      {0, 1, 10, 11, 12, 14, 22}
#define VOS_GPIO_SUGGEST_I2C  {10, 11, 0, 1, 12, 14, 22}

// ---- controlli di coerenza (si rompe la compilazione se il file e sbagliato) ----
static_assert(VOS_GPIO_COUNT == VOS_GPIO_MAX + 1, "VOS_GPIO_COUNT deve essere VOS_GPIO_MAX + 1");
static_assert(VOS_GPIO_EXISTS(VOS_PIN_LED_RGB) && VOS_GPIO_EXISTS(VOS_PIN_BOOT), "LED e tasto devono essere GPIO esistenti");
static_assert(VOS_GPIO_EXISTS(VOS_PIN_USB_DM) && VOS_GPIO_EXISTS(VOS_PIN_USB_DP), "pin USB devono essere GPIO esistenti");
static_assert(VOS_GPIO_EXISTS(VOS_I2C_SDA) && VOS_GPIO_EXISTS(VOS_I2C_SCL) && VOS_I2C_SDA != VOS_I2C_SCL, "I2C: pin non validi");
static_assert(VOS_I2C_SDA != VOS_PIN_LED_RGB && VOS_I2C_SCL != VOS_PIN_LED_RGB && VOS_I2C_SDA != VOS_PIN_BOOT && VOS_I2C_SCL != VOS_PIN_BOOT, "I2C: non sul LED o sul tasto");
static_assert(VOS_ADC2_FIRST <= VOS_ADC2_LAST && VOS_JTAG_FIRST <= VOS_JTAG_LAST && VOS_PSRAMQ_FIRST <= VOS_PSRAMQ_LAST, "avvisi: intervalli non validi");
static_assert(VOS_GPIO_EXISTS(VOS_PIN_UART_TX) && VOS_GPIO_EXISTS(VOS_PIN_UART_RX), "UART: pin non validi");
static_assert(VOS_ADC_FIRST <= VOS_ADC_LAST && VOS_GPIO_EXISTS(VOS_ADC_FIRST) && VOS_GPIO_EXISTS(VOS_ADC_LAST), "ADC: intervallo non valido");
#define VOS_SD_RESERVED(g) (VOS_GPIO_STRAP(g) || (g) == VOS_PIN_USB_DM || (g) == VOS_PIN_USB_DP || (g) == VOS_PIN_UART_TX || (g) == VOS_PIN_UART_RX || \
                            (g) == VOS_I2C_SDA || (g) == VOS_I2C_SCL || (g) == VOS_PIN_BOOT || (g) == VOS_PIN_LED_RGB || \
                            ((g) >= VOS_JTAG_FIRST && (g) <= VOS_JTAG_LAST) || ((g) >= VOS_PSRAMQ_FIRST && (g) <= VOS_PSRAMQ_LAST))
static_assert(VOS_GPIO_EXISTS(VOS_SD_DEF_CS) && VOS_GPIO_EXISTS(VOS_SD_DEF_MOSI) && VOS_GPIO_EXISTS(VOS_SD_DEF_SCK) && VOS_GPIO_EXISTS(VOS_SD_DEF_MISO), "microSD: pin inesistenti");
static_assert(VOS_SD_DEF_CS != VOS_SD_DEF_MOSI && VOS_SD_DEF_CS != VOS_SD_DEF_SCK && VOS_SD_DEF_CS != VOS_SD_DEF_MISO && VOS_SD_DEF_MOSI != VOS_SD_DEF_SCK && VOS_SD_DEF_MOSI != VOS_SD_DEF_MISO && VOS_SD_DEF_SCK != VOS_SD_DEF_MISO, "microSD: i quattro pin devono essere diversi");
static_assert(!VOS_SD_RESERVED(VOS_SD_DEF_CS) && !VOS_SD_RESERVED(VOS_SD_DEF_MOSI) && !VOS_SD_RESERVED(VOS_SD_DEF_SCK) && !VOS_SD_RESERVED(VOS_SD_DEF_MISO), "microSD: pin riservati dal sistema");
#if defined(VOS_PIN_LED2)
static_assert(VOS_GPIO_EXISTS(VOS_PIN_LED2) && VOS_PIN_LED2 != VOS_PIN_LED_RGB && VOS_PIN_LED2 != VOS_PIN_BOOT, "LED2: pin non valido");
#endif
static_assert(VOS_RTC_MEM_BYTES >= 4096 && !(VOS_HAS_ULP && VOS_HAS_LP_CPU), "capacita del registratore: valori non coerenti");
