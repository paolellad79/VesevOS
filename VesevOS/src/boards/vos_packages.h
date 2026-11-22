// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_packages.h
// STRATO 0 (boards): gli interruttori di compilazione dei package. Stanno qui, sotto i driver, perche anche i driver
// devono sapere se il loro package c'e (i driver non includono core/). vos_common.h li include, quindi il resto non cambia.
#pragma once
#include "vos_board.h"   // il file di scheda dice cosa il chip sa fare (VOS_HAS_BLE, VOS_HAS_WIFI): da li partono i valori di fabbrica

// PACKAGE (moduli opzionali): 1 = dentro il firmware, 0 = tolti (meno flash e meno RAM).
// Per toglierne uno basta mettere 0 e ricompilare: i servizi e le voci in pagina/shell spariscono da soli.
// Si cambiano qui sotto, oppure con -D nelle opzioni di compilazione (idf/main/CMakeLists.txt).
// BLE, MQTT e mesh seguono le capacita dichiarate dal file di scheda: nessun controllo per singolo chip nel codice comune.
// Il Bluetooth LE dipende da VOS_HAS_BLE; MQTT e ESP-NOW passano dal Wi-Fi, quindi da VOS_HAS_WIFI (l'H2 non lo ha).
#ifndef VOS_HAS_BLE
#define VOS_HAS_BLE 1
#endif
#ifndef VOS_HAS_WIFI
#define VOS_HAS_WIFI 1
#endif
#ifndef VOS_WITH_BLE
#define VOS_WITH_BLE VOS_HAS_BLE    // Bluetooth (NimBLE), spento: lo accende solo un'app con il permesso ble
#endif
#ifndef VOS_WITH_MQTT
#define VOS_WITH_MQTT VOS_HAS_WIFI  // client MQTT (broker, Home Assistant)
#endif
#ifndef VOS_WITH_MESH
#define VOS_WITH_MESH VOS_HAS_WIFI  // ESP-NOW tra schede
#endif
#ifndef VOS_WITH_MFA
#define VOS_WITH_MFA 1      // codice a due passaggi (TOTP) al login
#endif
#ifndef VOS_WITH_FUNZIONI
#define VOS_WITH_FUNZIONI 1    // script (interprete in third_party/funzioni; gira in un task con limiti)
#endif
#ifndef VOS_WITH_DRVCMD
#define VOS_WITH_DRVCMD 1   // comandi dei driver (sezioni [commands] e [cmd] dei file .vdb), scritti nelle funzioni VesevOS: serve VOS_WITH_FUNZIONI
#endif
#ifndef VOS_WITH_STATS
#define VOS_WITH_STATS 1    // statistiche d'uso (contatori locali)
#endif
#ifndef VOS_WITH_WPS
#define VOS_WITH_WPS 1      // collegamento alla Wi-Fi di casa con il WPS (pulsante o PIN). Se il link fallisce per simboli mancanti: mettere 0
#endif
#ifndef VOS_WITH_REC
#define VOS_WITH_REC 1      // registratore di dati (campiona periferiche, temperatura e RAM; CSV su flash o microSD; spento di fabbrica)
#endif
#ifndef VOS_WITH_SD
#define VOS_WITH_SD 1       // scheda microSD (modulo SPI, FAT32): montaggio a richiesta, mai nell'avvio
#endif
#if !defined(VOS_WITH_PWM) && defined(VOS_BOARD_PWM)   // vale solo se vos_board.h e gia stato incluso (vos_common.h lo fa): chi usa VOS_WITH_PWM include vos_common.h
#define VOS_WITH_PWM VOS_BOARD_PWM   // PWM dell'utente (periferica PWM, comando pwm, funzione pwm delle funzioni VesevOS). Di fabbrica lo decide il file di scheda (VOS_BOARD_PWM); il LED2 funziona anche a 0
#endif
#if !defined(VOS_WITH_CONTATORE) && defined(VOS_BOARD_CONTATORE)   // come il PWM: vale dopo vos_board.h
#define VOS_WITH_CONTATORE VOS_BOARD_CONTATORE   // contatore di impulsi dell'utente (periferica "contatore", comando contatore, funzione contatore delle funzioni VesevOS). Lo decide il file di scheda (VOS_BOARD_CONTATORE): serve il blocco PCNT, il C3 non lo ha
#endif
#if !defined(VOS_WITH_LED_RGB_A_MANO) && defined(VOS_BOARD_LED_RGB_MANO)   // come il PWM: vale dopo vos_board.h
#define VOS_WITH_LED_RGB_A_MANO VOS_BOARD_LED_RGB_MANO   // LED a colori WS2812 a mano dell'utente (periferica "led_rgb"): serve almeno un canale RMT di trasmissione libero oltre a quello del LED di bordo. Lo decide il file di scheda (VOS_BOARD_LED_RGB_MANO)
#endif
#if !defined(VOS_WITH_ULP_SOGLIE) && defined(VOS_HAS_ULP)   // come il PWM: vale dopo vos_board.h
#define VOS_WITH_ULP_SOGLIE VOS_HAS_ULP   // soglie del coprocessore ULP-RISC-V (tabella scritta dalla CPU, programma fisso nostro che sveglia la CPU). Solo S3: serve CONFIG_ULP_COPROC_TYPE_RISCV
#endif
#ifndef VOS_WITH_ELF
#if defined(CONFIG_IDF_TARGET_ESP32S3)
#define VOS_WITH_ELF 1      // moduli ELF (.vae/.vde): codice nativo caricato a richiesta. Dentro il firmware sull'ESP32-S3; il sottosistema vde (servizio runtime "elf") e ACCESO di fabbrica, si spegne dai Task
#else
#define VOS_WITH_ELF 0      // altri chip: il caricatore non c'e ancora (vedi docs/progetto/piano-integrazione-elf.md, sezione 6)
#endif
#endif
// Dove appare la microSD nei percorsi (punto di montaggio). "/sd" resta come scorciatoia di input (vedi core/vos_paths.h).
// Al massimo 15 caratteri: e il limite del percorso base dei file system virtuali di ESP-IDF (ESP_VFS_PATH_MAX).
#define VOS_SD_MOUNT_POINT "/media/sd"

// BACKEND dei driver (un driver = un contratto in drivers/vos_drv_*.h + un codice per ogni backend in drivers/<backend>/).
// Si cambia il backend senza toccare il resto. Oggi c'e solo idf = ESP-IDF nativo (dalla fase 5 non c'e piu il backend Arduino).
// Il backend "host" (prove sul computer) sta in tools/hosttest e non passa da qui.
#define VOS_BACKEND_IDF     2
#ifndef VOS_BACKEND
#define VOS_BACKEND VOS_BACKEND_IDF
#endif
