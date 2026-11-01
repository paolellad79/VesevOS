// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_packages.h
// STRATO 0 (boards): gli interruttori di compilazione dei package. Stanno qui, sotto i driver, perche anche i driver
// devono sapere se il loro package c'e (i driver non includono core/). vos_common.h li include, quindi il resto non cambia.
#pragma once

// PACKAGE (moduli opzionali): 1 = dentro il firmware, 0 = tolti (meno flash e meno RAM).
// Per toglierne uno basta mettere 0 e ricompilare: i servizi e le voci in pagina/shell spariscono da soli.
// Si cambiano qui sotto, oppure con -D nelle opzioni di compilazione (idf/main/CMakeLists.txt).
#ifndef VOS_WITH_BLE
#define VOS_WITH_BLE 1      // Bluetooth (NimBLE), spento: lo accende solo un'app Berry con il permesso ble
#endif
#ifndef VOS_WITH_MQTT
#define VOS_WITH_MQTT 1     // client MQTT (broker, Home Assistant)
#endif
#ifndef VOS_WITH_MESH
#define VOS_WITH_MESH 1     // ESP-NOW tra schede
#endif
#ifndef VOS_WITH_MFA
#define VOS_WITH_MFA 1      // codice a due passaggi (TOTP) al login
#endif
#ifndef VOS_WITH_BERRY
#define VOS_WITH_BERRY 1    // script Berry (interprete in third_party/berry; gira in un task con limiti)
#endif
#ifndef VOS_WITH_DRVCMD
#define VOS_WITH_DRVCMD 1   // comandi dei driver (sezioni [commands] e [cmd] dei file .vdb), scritti in Berry: serve VOS_WITH_BERRY
#endif
#ifndef VOS_WITH_STATS
#define VOS_WITH_STATS 1    // statistiche d'uso (contatori locali)
#endif
#ifndef VOS_WITH_WPS
#define VOS_WITH_WPS 1      // collegamento alla Wi-Fi di casa con il WPS (pulsante o PIN). Se il link fallisce per simboli mancanti: mettere 0
#endif
#ifndef VOS_WITH_SD
#define VOS_WITH_SD 1       // scheda microSD (modulo SPI, FAT32): montaggio a richiesta, mai nell'avvio
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
