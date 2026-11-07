// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_https.h
// STRATO 1 (driver): scarica un piccolo file da un indirizzo https, con il certificato del server controllato.
// Due usi: leggere gli elenchi degli aggiornamenti in memoria e scaricare il firmware a blocchi. Niente reindirizzamenti.
// Va chiamato da un task con stack abbondante, mai dal task del server web: il collegamento puo durare decine di secondi.
#pragma once
#include <stddef.h>
#include <stdint.h>

enum DrvHttpsEsito : uint8_t {
  HTTPS_OK = 0,
  HTTPS_E_MEMORIA,          // il client non parte: manca RAM
  HTTPS_E_COLLEGAMENTO,     // rete, certificato o ora sbagliata
  HTTPS_E_RISPOSTA,         // il server ha risposto con un codice diverso da 200
  HTTPS_E_TROPPO,           // il file non entra in buf
  HTTPS_E_TEMPO,            // lettura interrotta o troppo lunga
  HTTPS_E_SCRITTURA         // chi riceve i blocchi ha detto basta
};

// Riceve un blocco del file. false = fermare lo scaricamento.
typedef bool (*DrvHttpsBlocco)(void* ctx, const uint8_t* dati, size_t n);

// Mette il file in buf e lo chiude con 0 (cap comprende lo zero). n = byte letti, statoHttp = codice del server (0 se non ha risposto).
DrvHttpsEsito drvHttpsScarica(const char* url, char* buf, size_t cap, size_t& n, uint32_t tempoMs, int& statoHttp);
// Scarica un file grande a blocchi: buf e il buffer di lavoro, tetto i byte massimi ammessi. Tocca a chi riceve controllare il contenuto.
DrvHttpsEsito drvHttpsBlocchi(const char* url, uint8_t* buf, size_t cap, DrvHttpsBlocco ricevi, void* ctx, uint32_t tetto, uint32_t tempoMs, int& statoHttp);
