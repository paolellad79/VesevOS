// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_region.h
// Paese e regole radio: canali e potenza ammessi, antenna. I dati vengono dal file unico "common"
// (tools/mkcommon.py). Senza paese scelto valgono regole prudenti: canali 1-11 e limite 20 dBm EIRP (con l'antenna della scheda: 15 dBm al chip).
// Non e consulenza legale: chi vende un apparecchio deve verificare le norme del proprio paese.
#pragma once
#include <string>
#include <stddef.h>
#include <stdint.h>
#include "vos_heapbuf.h"

bool   regionValid(const char* cc);         // codice paese presente nella tabella radio /usr/share/vesevos/radio.txt (senza tabella valida: false)
int    regionChannels();                    // canali ammessi: 1..N (11 o 13; 11 anche se la tabella manca: ripiego prudente)
int    regionLimitDbm();                    // limite EIRP del paese (dBm)
int    regionGuadagnoDeciDbi();             // guadagno dell'antenna in uso (decimi di dBi): esterna scelta dall'utente o quella della scheda
int    regionMaxDbm();                      // massimo al chip: min(limite - guadagno antenna, 20 dBm), per difetto, mai sotto 2
int    regionTxDbm();                       // potenza in uso (scelta dall'utente, mai oltre il massimo)
bool   regionChannelOk(int ch);
void   regionApplyRadio();                  // applica paese e potenza al Wi-Fi (dopo WiFi.mode)
#define REGION_JSON_MAX 1280   // con tre formati scritti a mano di 32 caratteri
bool   regionJson(HeapBuf& out);            // stato per la pagina (JSON aggiunto in coda)
std::string regionText();                   // stato per la shell
// Avviso in audit (solo avviso, i valori restano applicati) se la tabella radio ha una riga oltre 20 dBm o oltre 13 canali. Si chiama una volta,
// dopo il controllo di radio.txt. Decisione dell'utente del 16/11/2026: nessun tetto nel parser.
void   regionTabellaAvviso();
