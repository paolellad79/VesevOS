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
#define VOS_SRAM_KB       512           // SRAM interna nominale del chip (ESP32-S3); la mappa della RAM (drvMemMap) parte da qui

// ---- antenna (dato dell'utente, 16/11/2026: CrossAir CA-C03 su tutte le SuperMini) ----
// Guadagno di picco dell'antenna della scheda in decimi di dBi (43 = 4,3 dBi; scheda del produttore). Serve a calcolare l'EIRP:
// il limite del paese comprende l'antenna, quindi la potenza al chip e il limite meno questo guadagno (core/vos_region_calc.h).
// Se la scheda ha un'antenna esterna scelta dall'utente vale il guadagno indicato da lui, non la somma.
#define VOS_ANTENNA_DECIMI_DBI  43

// ---- frequenza della CPU (limite e gradini per esp_pm; usati da core/vos_cpugov e drivers/vos_drv_cpu) ----
#define VOS_CPU_MHZ_MIN    80           // gradino basso (a riposo)
#define VOS_CPU_MHZ_MID    160          // gradino di mezzo
#define VOS_CPU_MHZ_MAX    240          // massimo del chip (S3)

// ---- cosa c'e sulla scheda ----
#define VOS_HAS_PSRAM        1
#define VOS_HAS_USB_NATIVE   1
#define VOS_HAS_BLE          1
#define VOS_HAS_WIFI         1
#define VOS_HAS_TEMP_SENSOR  1

// ---- LED e tasto ----
#define VOS_PIN_LED_RGB   48            // LED WS2812 integrato
#define VOS_LED_TYPE      "WS2812"
#define VOS_PIN_BOOT       0            // tasto fisico

// ---- periferiche di bordo: gia presenti sulla scheda, subito disponibili, senza driver (fonte unica) ----
// Una riga per voce: X(id, etichetta, colore, gruppo, scheda della pagina, funzione, widget, pin, nota del pin).
// id = classe + numero con trattino basso (decisione del 13/11/2026): "led_rgb1" (LED a colori), "led1" (LED monocromatico); ogni classe ha la sua numerazione.
// colore: campo del LED monocromatico mostrato nell'etichetta ("LED1 (blu)"); "" = nessuno (i LED a colori non lo hanno).
// funzione: LEDRGB (colore e modo), LEDONOFF (spento, acceso, lampeggia), TASTO (sola lettura).
// widget: come la pagina disegna la voce ("colore", "onoff", "tasto"). Servono ai file devices/vos_dev.cpp e devices/vos_pins.cpp.
#define VOS_BOARD_PERIF(X) \
  X("led_rgb1", "LED_RGB1",     "",     "Luce",     "led_rgb1", LEDRGB, "colore", VOS_PIN_LED_RGB, "LED RGB della scheda") \
  X("boot",     "Tasto fisico", "",     "Pulsante", "btn",      TASTO,  "tasto",  VOS_PIN_BOOT,    "Tasto fisico")

// ---- GPIO ----
// S3: i GPIO 22-25 non esistono, 26-32 sono flash/PSRAM; 33-37 sono liberi con PSRAM quad (SuperMini 2 MB)
#define VOS_GPIO_MAX       48           // numero piu alto
#define VOS_GPIO_COUNT     49           // = VOS_GPIO_MAX + 1 (dimensione delle tabelle indicizzate per GPIO)
#define VOS_GPIO_EXISTS(g) (((g) >= 0 && (g) <= 21) || ((g) >= 33 && (g) <= 48))
#define VOS_PIN_USB_DM    19            // USB D- (nativa)
#define VOS_PIN_USB_DP    20            // USB D+ (nativa)
#define VOS_GPIO_STRAP(g)  ((g) == 0 || (g) == 3 || (g) == 45 || (g) == 46)   // pin di avvio: meglio non toccarli (datasheet S3, tab. 3-1)

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

// ---- moduli ELF (package VOS_WITH_ELF): cosa puo fare questa scheda con il codice nativo caricato ----
// Valori provati in prova su scheda (prova ELF 3, 30/10/2026). Il driver (drivers/vos_drv_elf) li legge, il livello A (packages/vos_elf) rifiuta cio che non entra.
#define VOS_ELF_ARCH           1        // 1 = Xtensa, 2 = RISC-V
#define VOS_ELF_MACHINE        94       // e_machine accettato dall'ELF (94 = Xtensa, 243 = RISC-V)
#define VOS_ELF_ABI            1        // 1 = Xtensa con finestre di registri (quello dei moduli di serie)
#define VOS_ELF_HAS_PSRAM      1        // la scheda ha la PSRAM (la presenza vera si misura all'avvio)
#define VOS_ELF_EXEC_PSRAM     1        // il codice del modulo puo girare dalla PSRAM
#define VOS_ELF_RAM_MAX_MODULE 131072   // tetto di RAM per un modulo (byte)
#define VOS_ELF_FILE_MAX       131072   // tetto della dimensione del file ELF (byte)

// ---- PWM dell'utente (package VOS_WITH_PWM): 1 = dentro il firmware di fabbrica (decisione dell'utente 13/11: su tutte le schede con LEDC) ----
#define VOS_BOARD_PWM 1
// ---- Contatore di impulsi dell'utente (package VOS_WITH_CONTATORE): il chip ha PCNT (4 unita), codice ~8-9 KB stimati.
// ACCESO (decisione dell'utente 13/11/2026: "Aggiungi il contatore"): dopo la 1.8.22 il margine di app0 e 143.248 byte.
#define VOS_BOARD_CONTATORE 1
// ---- LED a colori WS2812 a mano (classe "led_rgb", package VOS_WITH_LED_RGB_A_MANO): l'S3 ha 4 canali RMT di trasmissione, 1 e del LED di bordo (SOC_RMT_TX_CANDIDATES_PER_GROUP, soc_caps.h IDF 5.5.5) ----
#define VOS_BOARD_LED_RGB_MANO 1

// ---- tabella dei piedini: da qui la pagina disegna lo schema della scheda (tab Pin) ----
// Una riga per piedino o piazzola: X(lato, posizione, gpio, tipo, bit). Significato dei campi: vedi vos_board.h.
// Fonte dei numeri: datasheet ESP32-S3 (tabelle dei pin e delle funzioni analogiche) e pinout della SuperMini di terza parte.
// DA VERIFICARE A VISTA sulla scheda: ordine dei piedini lungo i lati, 7 e 8 sul bordo corto, posizione delle piazzole del retro.
//   Sinistra (dall'alto): 43 44 1 2 3 4 5 6. Destra: 5V GND 3V3 13 12 11 10 9. Basso: 7 8.
//   Retro (da sotto): colonna esterna sinistra 36 35 21 18 17 16 15 14, interna 38 37 34 33, destra 48 47 46 45 42 41 40 39.
// Funzioni: ADC1 = GPIO1-10, ADC2 = GPIO11-20, tocco = GPIO1-14, RTC = GPIO0-21, SPI = 9-14, JTAG = 39-42, UART0 = 43/44.
// Avvio (strapping) = 0, 3, 45, 46: il GPIO0 e il tasto BOOT e non sta sul bordo.
#define VOS_BOARD_PINS(X) \
  X(VOS_PL_SIN, 0, 43, VOS_PT_GPIO, VOS_PF_UART | VOS_PF_RISERVATO) \
  X(VOS_PL_SIN, 1, 44, VOS_PT_GPIO, VOS_PF_UART | VOS_PF_RISERVATO) \
  X(VOS_PL_SIN, 2,  1, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_TOCCO | VOS_PF_RTC) \
  X(VOS_PL_SIN, 3,  2, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_TOCCO | VOS_PF_RTC) \
  X(VOS_PL_SIN, 4,  3, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_TOCCO | VOS_PF_RTC | VOS_PF_AVVIO) \
  X(VOS_PL_SIN, 5,  4, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_TOCCO | VOS_PF_RTC) \
  X(VOS_PL_SIN, 6,  5, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_TOCCO | VOS_PF_RTC) \
  X(VOS_PL_SIN, 7,  6, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_TOCCO | VOS_PF_RTC) \
  X(VOS_PL_DES, 0, -1, VOS_PT_5V, 0) \
  X(VOS_PL_DES, 1, -1, VOS_PT_GND, 0) \
  X(VOS_PL_DES, 2, -1, VOS_PT_3V3, 0) \
  X(VOS_PL_DES, 3, 13, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_TOCCO | VOS_PF_RTC | VOS_PF_SPI) \
  X(VOS_PL_DES, 4, 12, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_TOCCO | VOS_PF_RTC | VOS_PF_SPI) \
  X(VOS_PL_DES, 5, 11, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_TOCCO | VOS_PF_RTC | VOS_PF_SPI) \
  X(VOS_PL_DES, 6, 10, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_TOCCO | VOS_PF_RTC | VOS_PF_SPI) \
  X(VOS_PL_DES, 7,  9, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_TOCCO | VOS_PF_RTC | VOS_PF_SPI) \
  X(VOS_PL_BASSO, 0, 7, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_TOCCO | VOS_PF_RTC) \
  X(VOS_PL_BASSO, 1, 8, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_TOCCO | VOS_PF_RTC) \
  X(VOS_PL_RETRO_SIN, 0, 36, VOS_PT_GPIO, 0) \
  X(VOS_PL_RETRO_SIN, 1, 35, VOS_PT_GPIO, 0) \
  X(VOS_PL_RETRO_SIN, 2, 21, VOS_PT_GPIO, VOS_PF_RTC) \
  X(VOS_PL_RETRO_SIN, 3, 18, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_RTC) \
  X(VOS_PL_RETRO_SIN, 4, 17, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_RTC) \
  X(VOS_PL_RETRO_SIN, 5, 16, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_RTC) \
  X(VOS_PL_RETRO_SIN, 6, 15, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_RTC) \
  X(VOS_PL_RETRO_SIN, 7, 14, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_TOCCO | VOS_PF_RTC | VOS_PF_SPI) \
  X(VOS_PL_RETRO_INT, 0, 38, VOS_PT_GPIO, 0) \
  X(VOS_PL_RETRO_INT, 1, 37, VOS_PT_GPIO, 0) \
  X(VOS_PL_RETRO_INT, 2, 34, VOS_PT_GPIO, 0) \
  X(VOS_PL_RETRO_INT, 3, 33, VOS_PT_GPIO, 0) \
  X(VOS_PL_RETRO_DES, 0, 48, VOS_PT_GPIO, VOS_PF_LED) \
  X(VOS_PL_RETRO_DES, 1, 47, VOS_PT_GPIO, 0) \
  X(VOS_PL_RETRO_DES, 2, 46, VOS_PT_GPIO, VOS_PF_AVVIO) \
  X(VOS_PL_RETRO_DES, 3, 45, VOS_PT_GPIO, VOS_PF_AVVIO) \
  X(VOS_PL_RETRO_DES, 4, 42, VOS_PT_GPIO, VOS_PF_JTAG) \
  X(VOS_PL_RETRO_DES, 5, 41, VOS_PT_GPIO, VOS_PF_JTAG) \
  X(VOS_PL_RETRO_DES, 6, 40, VOS_PT_GPIO, VOS_PF_JTAG) \
  X(VOS_PL_RETRO_DES, 7, 39, VOS_PT_GPIO, VOS_PF_JTAG) \
  X(VOS_PL_RETRO_ALTO, 0, -1, VOS_PT_BATT_MENO, 0) \
  X(VOS_PL_RETRO_ALTO, 1, -1, VOS_PT_BATT_PIU, 0) \
  X(VOS_PL_RETRO_ALTO, 2, -1, VOS_PT_BOOST, 0)

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
