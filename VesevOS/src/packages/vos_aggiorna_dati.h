// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_aggiorna_dati.h
// Lettura difensiva dell'elenco degli aggiornamenti e calcolo dell'orario. Nessun accesso a rete o file: si prova anche sul computer.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <time.h>

struct AggDisponibile {
  char     ver[24];
  char     url[200];
  char     sha[65];
  uint32_t dim;
  char     data[11];       // AAAA-MM-GG, vuota se non dichiarata
  char     chi[33];        // chi ha pubblicato, vuoto se non dichiarato
  char     canale[16];     // "official" se ufficiale, vuoto se non dichiarato
  char     verRec[24];     // recovery pubblicato, vuoto se non dichiarato
};

// Legge aggiorna/latest.json. false = testo non valido, campo obbligatorio mancante o troppo lungo: d non va usato.
bool aggLeggiUltima(const char* testo, size_t n, AggDisponibile& d);
// Il primo blocco del file e un'immagine per ESP32-S3? Va bene almeno il byte 0xE9 e il codice del chip.
bool aggTestaFirmware(const uint8_t* dati, size_t n);
// Cerca il marchio del firmware a pezzi: pos si conserva tra un blocco e l'altro, parte da 0. true = trovato.
bool aggMarchio(const uint8_t* dati, size_t n, size_t& pos);
#define AGG_TENTATIVI_MAX 3
#define AGG_MARGINE_SD_KB 64
// Si puo ancora tentare di installare versione? ultima e fatti = l'ultima versione tentata e quante volte e gia andata nel recovery.
bool aggSpazioRiserva(uint32_t liberiKb, uint32_t byteImmagine);    // c'e posto per la copia di riserva?
bool aggTentativoAmmesso(const char* ultima, uint32_t fatti, const char* versione);
// La versione tentata e gia installata (o superata): i tentativi si dimenticano. installata = VOS_VERSION.
bool aggTentativiDaAzzerare(const char* ultima, const char* installata);
// E il minuto del controllo? freq: 0 giorno, 1 settimana (giorno 1 = lunedi), 2 mese. ora = minuti dalla mezzanotte.
bool aggOrarioGiusto(const struct tm& t, uint8_t freq, uint8_t giorno, uint16_t ora);
