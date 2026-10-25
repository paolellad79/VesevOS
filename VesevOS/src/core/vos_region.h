// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_region.h
// Paese e regole radio: canali e potenza ammessi, antenna. I dati vengono dal file unico "common"
// (tools/mkcommon.py). Senza paese scelto valgono regole prudenti: canali 1-11, 20 dBm.
// Non e consulenza legale: chi vende un apparecchio deve verificare le norme del proprio paese.
#pragma once
#include <string>
#include <stddef.h>
#include <stdint.h>
#include "vos_heapbuf.h"

bool   regionValid(const char* cc);         // codice paese conosciuto (ISO 3166, due lettere maiuscole)
int    regionChannels();                    // canali ammessi: 1..N (11 o 13)
int    regionLimitDbm();                    // limite EIRP del paese (dBm)
int    regionMaxDbm();                      // massimo reale: min(limite - guadagno antenna, 20 dBm del chip)
int    regionTxDbm();                       // potenza in uso (scelta dall'utente, mai oltre il massimo)
bool   regionChannelOk(int ch);
void   regionApplyRadio();                  // applica paese e potenza al Wi-Fi (dopo WiFi.mode)
#define REGION_JSON_MAX 768
bool   regionJson(HeapBuf& out);            // stato per la pagina (JSON aggiunto in coda)
std::string regionText();                   // stato per la shell
const uint8_t* regionCommonGz(size_t& len); // file "common" compresso (per la pagina)
