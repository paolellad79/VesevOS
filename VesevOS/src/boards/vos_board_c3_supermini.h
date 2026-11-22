// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - boards/vos_board_c3_supermini.h
// File di scheda: ESP32-C3 SuperMini (RISC-V un core, 4 MB flash nel package, 400 KB SRAM, niente PSRAM, USB seriale/JTAG,
// un solo LED blu semplice su GPIO8 (niente WS2812), solo antenna interna).
// Fonti: datasheet ESP32-C3 v2.4 (docs/terze-parti/esp32-c3/, cap. 2 "Pins" e cap. 3 "Boot Configurations") e, per la scheda,
// espboards.dev e il pinout locale (terze parti). Dove c'e scritto "DA VERIFICARE" il valore e prudente e va confermato con lo schema
// o a vista sulla scheda. Solo caratteri ASCII. Contiene solo cio che cambia rispetto a vos_board_c6_supermini.h / vos_board_s3_supermini.h.
#pragma once

#define VOS_BOARD         "ESP32-C3 SuperMini"
#define VOS_CHIP          "ESP32-C3"
#define VOS_CPU_SIGLA     "C3"                   // sigla nei nomi dei file e in aggiorna/latest.json (blocco "schede")
#define VOS_ADDR2LINE     "riscv32-esp-elf-addr2line"

// ---- memoria ----
#define VOS_FLASH_MB      4
#define VOS_PSRAM_MB      0
#define VOS_SRAM_KB       400           // SRAM interna nominale (datasheet C3, "Features": 400 KB, 16 KB usati dalla cache)

// ---- antenna (dato dell'utente, 16/11/2026: CrossAir CA-C03 su tutte le SuperMini) ----
// Guadagno di picco dell'antenna della scheda in decimi di dBi (43 = 4,3 dBi; scheda del produttore). Serve a calcolare l'EIRP:
// il limite del paese comprende l'antenna, quindi la potenza al chip e il limite meno questo guadagno (core/vos_region_calc.h).
// Se la scheda ha un'antenna esterna scelta dall'utente vale il guadagno indicato da lui, non la somma.
#define VOS_ANTENNA_DECIMI_DBI  43

// ---- frequenza della CPU (limite e gradini per esp_pm) ----
// C3: massimo 160 MHz (datasheet C3, "Features"). Gradino basso 80 MHz: come il C6, DA VERIFICARE con la guida IDF 5.5.5 (power management).
#define VOS_CPU_MHZ_MIN    80
#define VOS_CPU_MHZ_MID    160          // qui coincide con il massimo
#define VOS_CPU_MHZ_MAX    160

// ---- poca RAM e hotspot (decisione dell'utente 14/11/2026) ----
// Senza PSRAM e con ~200 KB di heap: modalita minima (vedi vos_board.h) e potenza ridotta dell'hotspot (la SuperMini C3 ha l'antenna SMD piccola e si scalda a
// piena potenza: fonti di terzi, NON verificato sulla scheda). 11 dBm = valore di partenza da misurare; resta sempre sotto il limite del paese.
#define VOS_RAM_MINIMA        1
#define VOS_LOG_RIGHE         40          // registro di sistema: 40 righe x 120 byte = 4,8 KB invece di 18 KB (prova 1.8.23f: RAM al 93%)
// Stack dei task: valori comuni a tutte le schede (vos_board.h), decisione dell'utente 15/11/2026: personalizzare il meno possibile.
#define VOS_AP_TXPOWER_DBM    11

// ---- cosa c'e sulla scheda ----
#define VOS_HAS_PSRAM        0
#define VOS_HAS_USB_NATIVE   1          // USB seriale/JTAG integrata (come il C6; non USB OTG)
#define VOS_HAS_BLE          1
#define VOS_HAS_WIFI         1
#define VOS_HAS_TEMP_SENSOR  1          // sensore di temperatura interno (datasheet C3, schema a blocchi)

// ---- LED e tasto ----
// Un solo LED blu semplice su GPIO8 (acceso/spento): niente LED RGB WS2812. Per questo VOS_HAS_LED_RGB vale 0 e il LED e il "LED2"
// del codice comune (modi, soglia, luminosita 0-255 con PWM). VOS_PIN_LED_RGB resta definito solo come pin del LED di bordo
// (la pagina Pin lo usa per segnare il LED); con VOS_HAS_LED_RGB = 0 nessun WS2812 viene pilotato su quel pin.
#define VOS_HAS_LED_RGB    0
#define VOS_PIN_LED_RGB    8            // LED blu di bordo (GPIO8 e anche pin di avvio: datasheet C3, tab. 3-1)
#define VOS_LED_TYPE      "LED"
#define VOS_PIN_LED2       8
// Livello attivo: VERIFICATO A VISTA dall'utente il 16/11/2026 sulla scheda vera (vesevos5) con il comando led1: con il valore 0 (1.8.24a)
// "led1 on" SPEGNEVA il LED e "led1 off" lo ACCENDEVA, quindi la polarita corretta e 1 = acceso a livello BASSO (come nelle fonti di terze
// parti sulla SuperMini C3). Il battito dello stato non e mai del tutto spento per progetto (luminosita 8-100%): "pulsa ma resta acceso"
// con il valore 1 era il comportamento NORMALE, non un'inversione. Il valore e usato una volta sola per livello: in drvPwmOpen
// (inversione nel calcolo del duty) e in drvPanicLed (livello diretto nel panico).
#define VOS_LED2_ACTIVE_LOW 1
#define VOS_LED_DIAG_PANICO 1           // il LED semplice (GPIO diretto) mostra anche il codice dentro il panico (vedi vos_board.h)
#define VOS_PIN_BOOT       9            // tasto BOOT (strapping, pull-up debole)

// ---- periferiche di bordo: gia presenti sulla scheda, subito disponibili, senza driver (fonte unica) ----
// Formato e significato dei campi: vedi vos_board_s3_supermini.h. Qui c'e solo il LED semplice blu: "led1", etichetta "LED1 (blu)";
// il codice che lo pilota e quello del LED monocromatico (funzione LEDONOFF).
#define VOS_BOARD_PERIF(X) \
  X("led1", "LED1",         "blu",  "Luce",     "led1",     LEDONOFF, "onoff",  VOS_PIN_LED2,    "LED della scheda") \
  X("boot", "Tasto fisico", "",     "Pulsante", "btn",      TASTO,    "tasto",  VOS_PIN_BOOT,    "Tasto fisico")

// ---- GPIO ----
// Il chip ha i GPIO 0-21 (datasheet C3, tab. 2-1). GPIO12-17 sono collegati alla flash nel package (cap. 2.3.3/2.3.4: da non usare),
// GPIO11 e VDD_SPI (alimentazione della flash). Sulla SuperMini escono 0-10, 20, 21 (e 18, 19 per la USB, non sul bordo).
#define VOS_GPIO_MAX       21
#define VOS_GPIO_COUNT     22
#define VOS_GPIO_EXISTS(g) (((g) >= 0 && (g) <= 10) || ((g) >= 18 && (g) <= 21))
#define VOS_PIN_USB_DM    18            // USB D- (datasheet C3, tab. 2-1)
#define VOS_PIN_USB_DP    19            // USB D+
#define VOS_GPIO_STRAP(g)  ((g) == 2 || (g) == 8 || (g) == 9)   // pin di avvio (datasheet C3, tab. 3-1 e cap. 2.3.3)

// ---- analogico e I2C ----
#define VOS_ADC_FIRST      0            // ADC1 del C3: GPIO0-4 (datasheet C3, tab. 2-1 e funzioni analogiche)
#define VOS_ADC_LAST       4
#define VOS_I2C_SDA        4            // scelta nostra, DA VERIFICARE: i pin di fabbrica 8/9 della SuperMini sono LED e tasto
#define VOS_I2C_SCL        5

// ---- microSD (nessun pin di fabbrica: modulo SPI esterno sui 4 pin liberi senza altre funzioni, DA VERIFICARE) ----
#define VOS_STR_(x) #x
#define VOS_STR(x) VOS_STR_(x)
#define VOS_HAS_SD        1
#define VOS_SD_DEF_CS          0
#define VOS_SD_DEF_MOSI        1
#define VOS_SD_DEF_SCK         3
#define VOS_SD_DEF_MISO       10
#define VOS_SD_DEF_LIST   VOS_STR(VOS_SD_DEF_CS) "," VOS_STR(VOS_SD_DEF_MOSI) "," VOS_STR(VOS_SD_DEF_SCK) "," VOS_STR(VOS_SD_DEF_MISO)

// ---- avvisi mostrati dalla pagina (solo testo, non bloccano nulla) ----
// ADC2 sul C3: solo GPIO5 (datasheet C3); con il Wi-Fi acceso la lettura non e affidabile (regola gia applicata su S3 e C6).
#define VOS_ADC2_FIRST     5
#define VOS_ADC2_LAST      5
#define VOS_JTAG_FIRST     4            // JTAG esterno del C3: MTMS 4, MTDI 5, MTCK 6, MTDO 7 (datasheet C3, tab. 2-1)
#define VOS_JTAG_LAST      7
#define VOS_PIN_UART_TX   21            // console seriale UART0 (U0TXD, pin 28; U0RXD e GPIO20)
#define VOS_PIN_UART_RX   20
#define VOS_PSRAMQ_FIRST  24            // il C3 non ha PSRAM: intervallo che punta a un pin inesistente
#define VOS_PSRAMQ_LAST   24

// ---- PWM dell'utente: il C3 ha LEDC (6 canali, datasheet C3 "Features"): dentro il firmware ----
#define VOS_BOARD_PWM 1
// ---- Contatore di impulsi dell'utente: il C3 NON ha il blocco PCNT (SOC_PCNT_SUPPORTED assente): spento ----
#define VOS_BOARD_CONTATORE 0
// Capacita del chip per il registratore di dati (solo dichiarazioni: il sonno profondo arriva dopo). C3: nessun coprocessore, niente PCNT, RTC 8 KB.
#define VOS_HAS_ULP          0   // coprocessore ULP (solo S3)
#define VOS_HAS_LP_CPU       0   // CPU a basso consumo (solo C6)
#define VOS_HAS_PCNT         0   // contatore di impulsi (PCNT)
#define VOS_RTC_MEM_BYTES    8192   // memoria che resta accesa nel sonno profondo
// ---- LED a colori WS2812 a mano (classe "led_rgb"): il C3 ha 2 canali RMT di trasmissione e nessun LED a colori di bordo: ne restano 2 ----
#define VOS_BOARD_LED_RGB_MANO 1

// ---- moduli ELF: il C3 e RISC-V, ma VOS_WITH_ELF resta 1 solo sull'S3 (vos_packages.h); valori prudenti come il C6 ----
#define VOS_ELF_ARCH           2        // 1 = Xtensa, 2 = RISC-V
#define VOS_ELF_MACHINE        243      // RISC-V
#define VOS_ELF_ABI            0
#define VOS_ELF_HAS_PSRAM      0
#define VOS_ELF_EXEC_PSRAM     0
#define VOS_ELF_RAM_MAX_MODULE 32768    // 400 KB di RAM e niente PSRAM: tetto prudente, DA VERIFICARE se un giorno si accende l'ELF
#define VOS_ELF_FILE_MAX       65536

// ---- tabella dei piedini: da qui la pagina disegna lo schema della scheda (tab Pin) ----
// X(lato, posizione, gpio, tipo, bit). Significato dei campi: vedi vos_board.h.
// Fonte dei numeri: datasheet ESP32-C3 (tab. 2-1) e pinout locale della SuperMini (terze parti, ricavato dalle coordinate del disegno).
// DA VERIFICARE A VISTA sulla scheda: l'ordine dei piedini lungo i lati (e il meno sicuro dei quattro chip).
//   Sinistra (dall'alto, USB in alto): 5 6 7 8 9 10 20 21. Destra: 5V GND 3V3 4 3 2 1 0.
// Funzioni: ADC1 = GPIO0-4, ADC2 = GPIO5, RTC = GPIO0-5, SPI (FSPI) = 2, 4-7, JTAG = 4-7, UART0 = 20/21, USB = 18/19 (non sul bordo).
// Avvio (strapping) = 2, 8, 9 (datasheet C3, tab. 3-1). Nessun sensore a tocco, nessun pad batteria sulla scheda base.
#define VOS_BOARD_PINS(X) \
  X(VOS_PL_SIN, 0,  5, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_RTC | VOS_PF_SPI | VOS_PF_JTAG) \
  X(VOS_PL_SIN, 1,  6, VOS_PT_GPIO, VOS_PF_SPI | VOS_PF_JTAG) \
  X(VOS_PL_SIN, 2,  7, VOS_PT_GPIO, VOS_PF_SPI | VOS_PF_JTAG) \
  X(VOS_PL_SIN, 3,  8, VOS_PT_GPIO, VOS_PF_AVVIO | VOS_PF_LED) \
  X(VOS_PL_SIN, 4,  9, VOS_PT_GPIO, VOS_PF_AVVIO | VOS_PF_TASTO) \
  X(VOS_PL_SIN, 5, 10, VOS_PT_GPIO, 0) \
  X(VOS_PL_SIN, 6, 20, VOS_PT_GPIO, VOS_PF_UART | VOS_PF_RISERVATO) \
  X(VOS_PL_SIN, 7, 21, VOS_PT_GPIO, VOS_PF_UART | VOS_PF_RISERVATO) \
  X(VOS_PL_DES, 0, -1, VOS_PT_5V, 0) \
  X(VOS_PL_DES, 1, -1, VOS_PT_GND, 0) \
  X(VOS_PL_DES, 2, -1, VOS_PT_3V3, 0) \
  X(VOS_PL_DES, 3,  4, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_RTC | VOS_PF_SPI | VOS_PF_JTAG) \
  X(VOS_PL_DES, 4,  3, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_RTC) \
  X(VOS_PL_DES, 5,  2, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_RTC | VOS_PF_SPI | VOS_PF_AVVIO) \
  X(VOS_PL_DES, 6,  1, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_RTC) \
  X(VOS_PL_DES, 7,  0, VOS_PT_GPIO, VOS_PF_ADC | VOS_PF_RTC)

// ---- pin consigliati: solo i liberi sicuri (0, 1, 3, 10), poi quelli del JTAG (4-7) che servono solo con il debug esterno ----
#define VOS_GPIO_SUGGEST      {0, 1, 3, 10, 4, 5, 6, 7}
#define VOS_GPIO_SUGGEST_I2C  {4, 5, 0, 1, 3, 10, 6, 7}

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
static_assert(VOS_GPIO_EXISTS(VOS_PIN_LED2) && VOS_PIN_LED2 != VOS_PIN_BOOT, "LED2: pin non valido");
static_assert(!VOS_HAS_LED_RGB && VOS_PIN_LED_RGB == VOS_PIN_LED2, "C3: nessun LED RGB, VOS_PIN_LED_RGB e solo il pin del LED di bordo");
static_assert(VOS_RTC_MEM_BYTES >= 4096 && !(VOS_HAS_ULP && VOS_HAS_LP_CPU), "capacita del registratore: valori non coerenti");
