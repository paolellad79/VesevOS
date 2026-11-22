// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - boards/vos_board_c6_supermini.h
// File di scheda: ESP32-C6 SuperMini (RISC-V, 4 MB flash, niente PSRAM, USB seriale/JTAG, LED WS2812 su GPIO8).
// Preliminare (prova di porting): i pin vengono da fonte di terza parte (espboards.dev) e dal datasheet del chip;
// dove c'e scritto "da verificare" il valore e prudente e va confermato con lo schema della scheda. Solo caratteri ASCII.
// Contiene solo cio che cambia rispetto al file dell'S3 (vos_board_s3_supermini.h), che e il modello per le voci.
#pragma once

#define VOS_BOARD         "ESP32-C6 SuperMini"
#define VOS_CHIP          "ESP32-C6"
#define VOS_CPU_SIGLA     "C6"                   // sigla nei nomi dei file e in aggiorna/latest.json (blocco "schede")
#define VOS_ADDR2LINE     "riscv32-esp-elf-addr2line"

// ---- memoria ----
#define VOS_FLASH_MB      4
#define VOS_PSRAM_MB      0
#define VOS_SRAM_KB       512           // SRAM interna nominale del chip (HP SRAM del C6)

// ---- antenna (dato dell'utente, 16/11/2026: CrossAir CA-C03 su tutte le SuperMini) ----
// Guadagno di picco dell'antenna della scheda in decimi di dBi (43 = 4,3 dBi; scheda del produttore). Serve a calcolare l'EIRP:
// il limite del paese comprende l'antenna, quindi la potenza al chip e il limite meno questo guadagno (core/vos_region_calc.h).
// Se la scheda ha un'antenna esterna scelta dall'utente vale il guadagno indicato da lui, non la somma.
#define VOS_ANTENNA_DECIMI_DBI  43

// ---- frequenza della CPU (limite e gradini per esp_pm; usati da core/vos_cpugov e drivers/vos_drv_cpu) ----
// C6: massimo 160 MHz (PLL 160). Gradini 80 e 160: da verificare con la guida IDF 5.5.5 del C6 (power management).
#define VOS_CPU_MHZ_MIN    80           // gradino basso (a riposo)
#define VOS_CPU_MHZ_MID    160          // gradino di mezzo (qui coincide con il massimo)
#define VOS_CPU_MHZ_MAX    160          // massimo del chip (C6)

// ---- cosa c'e sulla scheda ----
#define VOS_HAS_PSRAM        0
#define VOS_HAS_USB_NATIVE   1          // USB seriale/JTAG integrata (non USB OTG come l'S3)
#define VOS_HAS_BLE          1
#define VOS_HAS_WIFI         1
#define VOS_HAS_TEMP_SENSOR  1

// ---- LED e tasto ----
#define VOS_PIN_LED_RGB    8            // LED WS2812 integrato (strapping: lo usa anche l'avvio)
#define VOS_LED_TYPE      "WS2812"
#define VOS_LED_ORDER      VOS_LED_ORD_RGB   // ordine dei byte del LED RGB: sulla C6 e RGB (prova utente 08/11: con GRB rosso e verde erano scambiati); riprova: shell "led test"
// Secondo LED, semplice uscita su GPIO15 (colore da verificare: rosso o blu; fonte di terza parte). GPIO15 e un pin di strapping:
// si usa come uscita solo DOPO l'avvio (il livello e letto dal chip al reset, non lo forziamo mai prima).
#define VOS_PIN_LED2      15
#define VOS_LED2_ACTIVE_LOW 0           // 0 = acceso a livello alto: DA VERIFICARE con lo schema
#define VOS_LED_DIAG_PANICO 1           // il LED2 (GPIO diretto) mostra anche il codice dentro il panico (vedi vos_board.h)
#define VOS_PIN_BOOT       9            // tasto BOOT (strapping)

// ---- periferiche di bordo: gia presenti sulla scheda, subito disponibili, senza driver (fonte unica) ----
// Formato e significato dei campi: vedi vos_board_s3_supermini.h. Qui c'e in piu il LED monocromatico "led1" (secondo LED, GPIO15; colore da verificare).
#define VOS_BOARD_PERIF(X) \
  X("led_rgb1", "LED_RGB1",     "",     "Luce",     "led_rgb1", LEDRGB,   "colore", VOS_PIN_LED_RGB, "LED RGB della scheda") \
  X("led1",     "LED1",         "",     "Luce",     "led1",     LEDONOFF, "onoff",  VOS_PIN_LED2,    "LED della scheda") \
  X("boot",     "Tasto fisico", "",     "Pulsante", "btn",      TASTO,    "tasto",  VOS_PIN_BOOT,    "Tasto fisico")

// ---- GPIO ----
// Il chip ha i GPIO 0-30; sulla scheda si usano 0-23 (i successivi sono flash). Da verificare con lo schema.
#define VOS_GPIO_MAX       23
#define VOS_GPIO_COUNT     24
#define VOS_GPIO_EXISTS(g) ((g) >= 0 && (g) <= 23)
#define VOS_PIN_USB_DM    12            // USB D-
#define VOS_PIN_USB_DP    13            // USB D+
#define VOS_GPIO_STRAP(g)  ((g) == 4 || (g) == 5 || (g) == 8 || (g) == 9 || (g) == 15)   // pin di avvio: meglio non toccarli (datasheet C6, tab. 3-1)

// ---- analogico e I2C ----
#define VOS_ADC_FIRST      0            // ADC1 del C6: GPIO0-6
#define VOS_ADC_LAST       6
#define VOS_I2C_SDA        2            // da verificare: pin liberi sicuri, diversi da LED (8) e tasto (9)
#define VOS_I2C_SCL        3

// ---- microSD (nessun pin di fabbrica sulla scheda: modulo SPI esterno su pin liberi, da verificare) ----
// Come per l'S3 e una periferica di io.cfg: parte solo se l'utente la configura. Qui solo i pin proposti.
#define VOS_STR_(x) #x
#define VOS_STR(x) VOS_STR_(x)
#define VOS_HAS_SD        1
#define VOS_SD_DEF_CS         18
#define VOS_SD_DEF_MOSI       19
#define VOS_SD_DEF_SCK        20
#define VOS_SD_DEF_MISO       21
#define VOS_SD_DEF_LIST   VOS_STR(VOS_SD_DEF_CS) "," VOS_STR(VOS_SD_DEF_MOSI) "," VOS_STR(VOS_SD_DEF_SCK) "," VOS_STR(VOS_SD_DEF_MISO)

// ---- avvisi mostrati dalla pagina (solo testo, non bloccano nulla) ----
// Il C6 non ha ADC2 ne PSRAM: gli intervalli puntano a un pin che non esiste (24), cosi' non scattano mai.
#define VOS_ADC2_FIRST    24
#define VOS_ADC2_LAST     24
#define VOS_JTAG_FIRST     4            // JTAG esterno del C6: GPIO4-7 (da verificare)
#define VOS_JTAG_LAST      7
#define VOS_PIN_UART_TX   16            // console seriale UART0
#define VOS_PIN_UART_RX   17
#define VOS_PSRAMQ_FIRST  24
#define VOS_PSRAMQ_LAST   24

// ---- PWM dell'utente (package VOS_WITH_PWM): 1 = dentro il firmware di fabbrica (app0 del C6 ha molto margine) ----
#define VOS_BOARD_PWM 1
// ---- Contatore di impulsi dell'utente (package VOS_WITH_CONTATORE): serve il blocco PCNT (4 unita sul C6) ----
#define VOS_BOARD_CONTATORE 1
// Capacita del chip per il registratore di dati (solo dichiarazioni: il sonno profondo arriva dopo). C6: CPU LP, LP SRAM 16 KB.
#define VOS_HAS_ULP          0   // coprocessore ULP (solo S3)
#define VOS_HAS_LP_CPU       1   // CPU a basso consumo (solo C6)
#define VOS_HAS_PCNT         1   // contatore di impulsi (PCNT)
#define VOS_RTC_MEM_BYTES    16384   // memoria che resta accesa nel sonno profondo
// ---- LED a colori WS2812 a mano (classe "led_rgb"): il C6 ha 2 canali RMT di trasmissione, 1 e del LED di bordo: ne resta 1 ----
#define VOS_BOARD_LED_RGB_MANO 1

// ---- moduli ELF: il C6 e RISC-V, i moduli di serie sono Xtensa (da rifare); valori prudenti, da verificare ----
#define VOS_ELF_ARCH           2        // 1 = Xtensa, 2 = RISC-V
#define VOS_ELF_MACHINE        243      // RISC-V
#define VOS_ELF_ABI            0        // nessuna finestra di registri
#define VOS_ELF_HAS_PSRAM      0
#define VOS_ELF_EXEC_PSRAM     0
#define VOS_ELF_RAM_MAX_MODULE 65536    // meno RAM dell'S3 (niente PSRAM)
#define VOS_ELF_FILE_MAX       65536

// ---- tabella dei piedini: da qui la pagina disegna lo schema della scheda (tab Pin) ----
// Una riga per piedino o piazzola: X(lato, posizione, gpio, tipo, bit). Significato dei campi: vedi vos_board.h.
// Fonte dei numeri: datasheet ESP32-C6 (tabelle dei pin) e pinout della SuperMini di terza parte.
// DA VERIFICARE A VISTA sulla scheda: ordine dei piedini lungo i lati e posizione/lato delle piazzole 21, 22, 23, 12, 13 e BAT.
//   Sinistra (dall'alto): 16 17 0 1 2 3 4 5 6 7. Destra: 5V GND 3V3 20 19 18 15 14 9 8.
// Funzioni: ADC1 = GPIO0-6, LP/RTC = GPIO0-7, SPI = 2, 4-7 e 16-21, JTAG = 4-7, UART0 = 16/17, USB = 12/13.
// Avvio (strapping) = 4, 5, 8, 9, 15 (datasheet C6, tabella 3-1 e sezione 2.3.4).
#define VOS_BOARD_PINS(X) \
  X(VOS_PL_SIN, 0, 16, VOS_PT_GPIO, VOS_PF_UART | VOS_PF_SPI | VOS_PF_RISERVATO) \
  X(VOS_PL_SIN, 1, 17, VOS_PT_GPIO, VOS_PF_UART | VOS_PF_SPI | VOS_PF_RISERVATO) \
  X(VOS_PL_SIN, 2,  0, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_RTC) \
  X(VOS_PL_SIN, 3,  1, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_RTC) \
  X(VOS_PL_SIN, 4,  2, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_RTC | VOS_PF_SPI) \
  X(VOS_PL_SIN, 5,  3, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_RTC) \
  X(VOS_PL_SIN, 6,  4, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_RTC | VOS_PF_SPI | VOS_PF_JTAG | VOS_PF_AVVIO) \
  X(VOS_PL_SIN, 7,  5, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_RTC | VOS_PF_SPI | VOS_PF_JTAG | VOS_PF_AVVIO) \
  X(VOS_PL_SIN, 8,  6, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_RTC | VOS_PF_SPI | VOS_PF_JTAG) \
  X(VOS_PL_SIN, 9,  7, VOS_PT_GPIO, VOS_PF_RTC | VOS_PF_SPI | VOS_PF_JTAG) \
  X(VOS_PL_DES, 0, -1, VOS_PT_5V, 0) \
  X(VOS_PL_DES, 1, -1, VOS_PT_GND, 0) \
  X(VOS_PL_DES, 2, -1, VOS_PT_3V3, 0) \
  X(VOS_PL_DES, 3, 20, VOS_PT_GPIO, VOS_PF_SPI) \
  X(VOS_PL_DES, 4, 19, VOS_PT_GPIO, VOS_PF_SPI) \
  X(VOS_PL_DES, 5, 18, VOS_PT_GPIO, VOS_PF_SPI) \
  X(VOS_PL_DES, 6, 15, VOS_PT_GPIO, VOS_PF_AVVIO | VOS_PF_LED) \
  X(VOS_PL_DES, 7, 14, VOS_PT_GPIO, 0) \
  X(VOS_PL_DES, 8,  9, VOS_PT_GPIO, VOS_PF_AVVIO | VOS_PF_TASTO) \
  X(VOS_PL_DES, 9,  8, VOS_PT_GPIO, VOS_PF_AVVIO | VOS_PF_LED) \
  X(VOS_PL_RETRO_SIN, 0, 21, VOS_PT_GPIO, VOS_PF_SPI) \
  X(VOS_PL_RETRO_SIN, 1, 22, VOS_PT_GPIO, 0) \
  X(VOS_PL_RETRO_SIN, 2, 23, VOS_PT_GPIO, 0) \
  X(VOS_PL_RETRO_DES, 0, 12, VOS_PT_GPIO, VOS_PF_USB | VOS_PF_RISERVATO) \
  X(VOS_PL_RETRO_DES, 1, 13, VOS_PT_GPIO, VOS_PF_USB | VOS_PF_RISERVATO) \
  X(VOS_PL_RETRO_ALTO, 0, -1, VOS_PT_BATT_MENO, 0) \
  X(VOS_PL_RETRO_ALTO, 1, -1, VOS_PT_BATT_PIU, 0)

// ---- pin consigliati: solo i liberi sicuri (0-3, 14, 18-23), SDA e SCL in testa alla lista I2C ----
#define VOS_GPIO_SUGGEST      {0, 1, 2, 3, 14, 18, 19, 20, 21, 22, 23}
#define VOS_GPIO_SUGGEST_I2C  {2, 3, 0, 1, 14, 18, 19, 20, 21, 22, 23}

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
