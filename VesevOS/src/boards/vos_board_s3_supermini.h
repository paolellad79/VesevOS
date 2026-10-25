// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - boards/vos_board_s3_supermini.h
// File di scheda: ESP32-S3 SuperMini (4 MB flash, 2 MB PSRAM, USB nativa, LED WS2812 su GPIO48, solo antenna interna).
// Qui stanno TUTTI i numeri che dipendono dalla scheda (pin, memoria, funzioni presenti). Solo caratteri ASCII.
#pragma once

#define VOS_BOARD         "ESP32-S3 SuperMini"   // nome mostrato (tabella dei piedini nella pagina)
#define VOS_CHIP          "ESP32-S3"             // nome del chip
#define VOS_ADDR2LINE     "xtensa-esp32s3-elf-addr2line"   // programma che trasforma gli indirizzi di un crash in file e riga

// ---- memoria ----
#define VOS_FLASH_MB      4
#define VOS_PSRAM_MB      2

// ---- cosa c'e sulla scheda ----
#define VOS_HAS_PSRAM        1
#define VOS_HAS_USB_NATIVE   1
#define VOS_HAS_BLE          1
#define VOS_HAS_WIFI         1
#define VOS_HAS_TEMP_SENSOR  1

// ---- LED e tasto ----
#define VOS_PIN_LED_RGB   48            // LED WS2812 integrato
#define VOS_LED_TYPE      "WS2812"
#define VOS_PIN_BOOT       0            // pulsante BOOT

// ---- GPIO ----
// S3: i GPIO 22-25 non esistono, 26-32 sono flash/PSRAM; 33-37 sono liberi con PSRAM quad (SuperMini 2 MB)
#define VOS_GPIO_MAX       48           // numero piu alto
#define VOS_GPIO_COUNT     49           // = VOS_GPIO_MAX + 1 (dimensione delle tabelle indicizzate per GPIO)
#define VOS_GPIO_EXISTS(g) (((g) >= 0 && (g) <= 21) || ((g) >= 33 && (g) <= 48))
#define VOS_PIN_USB_DM    19            // USB D- (nativa)
#define VOS_PIN_USB_DP    20            // USB D+ (nativa)
#define VOS_GPIO_STRAP(g)  ((g) == 3 || (g) == 45 || (g) == 46)   // pin di avvio: meglio non toccarli

// ---- analogico e I2C ----
#define VOS_ADC_FIRST      1            // ADC1 (gli altri disturbano il Wi-Fi)
#define VOS_ADC_LAST      10
#define VOS_I2C_SDA        8            // pin di fabbrica di Wire
#define VOS_I2C_SCL        9

// ---- microSD (modulo SPI a 3,3 V, 6 pin: VCC GND MISO MOSI SCK CS; prova con il modulo marcato v474) ----
// La microSD e una periferica di io.cfg (classe "sd"): l'utente sceglie i 4 pin dalla pagina, controllati come per le altre periferiche.
// Qui ci sono solo i pin PROPOSTI di default: quelli "di fabbrica" del bus SPI2 (FSPI) dell'S3, vicini sul bordo destro (GPIO10-13), e la velocita.
// Non sono ADC2 critici, ne di avvio (3/45/46), ne USB, ne console, ne JTAG, ne I2C, ne flash/PSRAM.
#define VOS_STR_(x) #x
#define VOS_STR(x) VOS_STR_(x)
#define VOS_HAS_SD        1
#define VOS_SD_DEF_CS         10
#define VOS_SD_DEF_MOSI       11
#define VOS_SD_DEF_SCK        12
#define VOS_SD_DEF_MISO       13
#define VOS_SD_DEF_LIST   VOS_STR(VOS_SD_DEF_CS) "," VOS_STR(VOS_SD_DEF_MOSI) "," VOS_STR(VOS_SD_DEF_SCK) "," VOS_STR(VOS_SD_DEF_MISO)   // per la pagina

// ---- avvisi mostrati dalla pagina (solo testo, non bloccano nulla) ----
#define VOS_ADC2_FIRST    11            // ADC2: con il Wi-Fi acceso la lettura non e affidabile
#define VOS_ADC2_LAST     18
#define VOS_JTAG_FIRST    39            // usati dal debug JTAG se lo si usa
#define VOS_JTAG_LAST     42
#define VOS_PIN_UART_TX   43            // console seriale
#define VOS_PIN_UART_RX   44
#define VOS_PSRAMQ_FIRST  33            // liberi con PSRAM quad (2 MB), occupati con PSRAM octal
#define VOS_PSRAMQ_LAST   37

// ---- disegno della scheda nella pagina (JSON) ----
// fl/fr = file di piedini sinistra/destra viste dal fronte, dall'alto verso il basso (numero = GPIO, testo = alimentazione);
// bl/br = piazzole del retro, colonna sinistra/destra viste da sotto. Ogni GPIO esistente va in una sola lista oppure in nessuna (solo inventario).
#define VOS_BOARD_LAYOUT  "{\"fl\":[43,44,1,2,3,4,5,6,7],\"fr\":[\"5V\",\"GND\",\"3V3\",13,12,11,10,9,8]," \
                          "\"bl\":[38,37,34,33,21,18,17,16,15,14],\"br\":[36,35,48,47,46,45,42,41,40,39]}"

// ---- pin consigliati, in ordine: prima quelli comodi del bordo, per ultimi quelli meno usati ----
#define VOS_GPIO_SUGGEST      {1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 21, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 47}
#define VOS_GPIO_SUGGEST_I2C  {8, 9, 4, 5, 6, 7, 1, 2, 10, 11, 12, 13, 14, 15, 16, 17, 18, 21, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 47}

// ---- controlli di coerenza (si rompe la compilazione se il file e sbagliato) ----
static_assert(VOS_GPIO_COUNT == VOS_GPIO_MAX + 1, "VOS_GPIO_COUNT deve essere VOS_GPIO_MAX + 1");
static_assert(VOS_GPIO_EXISTS(VOS_PIN_LED_RGB) && VOS_GPIO_EXISTS(VOS_PIN_BOOT), "LED e tasto devono essere GPIO esistenti");
static_assert(VOS_GPIO_EXISTS(VOS_PIN_USB_DM) && VOS_GPIO_EXISTS(VOS_PIN_USB_DP), "pin USB devono essere GPIO esistenti");
static_assert(VOS_GPIO_EXISTS(VOS_I2C_SDA) && VOS_GPIO_EXISTS(VOS_I2C_SCL) && VOS_I2C_SDA != VOS_I2C_SCL, "I2C: pin non validi");
static_assert(VOS_ADC2_FIRST <= VOS_ADC2_LAST && VOS_JTAG_FIRST <= VOS_JTAG_LAST && VOS_PSRAMQ_FIRST <= VOS_PSRAMQ_LAST, "avvisi: intervalli non validi");
static_assert(VOS_GPIO_EXISTS(VOS_PIN_UART_TX) && VOS_GPIO_EXISTS(VOS_PIN_UART_RX), "UART: pin non validi");
static_assert(VOS_ADC_FIRST <= VOS_ADC_LAST && VOS_GPIO_EXISTS(VOS_ADC_FIRST) && VOS_GPIO_EXISTS(VOS_ADC_LAST), "ADC: intervallo non valido");
// microSD: quattro pin diversi, esistenti, e mai quelli riservati (avvio, USB, console, JTAG, I2C, tasto, LED, flash/PSRAM)
#define VOS_SD_RESERVED(g) (VOS_GPIO_STRAP(g) || (g) == VOS_PIN_USB_DM || (g) == VOS_PIN_USB_DP || (g) == VOS_PIN_UART_TX || (g) == VOS_PIN_UART_RX || \
                            (g) == VOS_I2C_SDA || (g) == VOS_I2C_SCL || (g) == VOS_PIN_BOOT || (g) == VOS_PIN_LED_RGB || \
                            ((g) >= VOS_JTAG_FIRST && (g) <= VOS_JTAG_LAST) || ((g) >= VOS_PSRAMQ_FIRST && (g) <= VOS_PSRAMQ_LAST))
static_assert(VOS_GPIO_EXISTS(VOS_SD_DEF_CS) && VOS_GPIO_EXISTS(VOS_SD_DEF_MOSI) && VOS_GPIO_EXISTS(VOS_SD_DEF_SCK) && VOS_GPIO_EXISTS(VOS_SD_DEF_MISO), "microSD: pin inesistenti");
static_assert(VOS_SD_DEF_CS != VOS_SD_DEF_MOSI && VOS_SD_DEF_CS != VOS_SD_DEF_SCK && VOS_SD_DEF_CS != VOS_SD_DEF_MISO && VOS_SD_DEF_MOSI != VOS_SD_DEF_SCK && VOS_SD_DEF_MOSI != VOS_SD_DEF_MISO && VOS_SD_DEF_SCK != VOS_SD_DEF_MISO, "microSD: i quattro pin devono essere diversi");
static_assert(!VOS_SD_RESERVED(VOS_SD_DEF_CS) && !VOS_SD_RESERVED(VOS_SD_DEF_MOSI) && !VOS_SD_RESERVED(VOS_SD_DEF_SCK) && !VOS_SD_RESERVED(VOS_SD_DEF_MISO), "microSD: pin riservati dal sistema");
