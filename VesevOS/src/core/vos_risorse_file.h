// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_risorse_file.h
// File singoli di /usr/share/vesevos: dati comuni, licenze minori, manuale, tabella radio. Il controllo e quello di SHA256SUMS (elenco ben
// formato con i soli nomi conosciuti, ogni file con il suo SHA-256; nessuna ancora al firmware dal 16/11/2026). Formato dei file: tools/mkrisorse.py.
#pragma once
#include "vos_risorse_rule.h"
#include "vos_risorse_elenco_rule.h"
#include "../drivers/vos_drv_file.h"

#define RIS_DIR "/usr/share/vesevos"
#define RIS_PERCORSO_MAX 64                  // byte per un percorso di questa cartella (il piu lungo: 41)

// ---- File singoli (non .vrs): dati comuni, licenze minori, manuale, tabella radio. Il controllo e quello di SHA256SUMS: l'elenco e ben formato
// e ogni file ha il suo SHA-256 scritto li. Il controllo completo si fa la PRIMA volta che un file serve dopo l'avvio e si ricorda in
// RAM (un bit per file). File mancante o non valido: l'errore dice quale, e chi chiama usa il suo ripiego.
enum RisFile : uint8_t { RF_COMUNI, RF_LIC_APACHE2, RF_LIC_BSD3, RF_LIC_MIT, RF_MANUALE, RF_RADIO, RF_HTML, RF_N };

const char* risFileNome(RisFile f);                                     // "comuni.json.gz", "manuale.txt", ...
bool        risNomeNotoFw(const char* nome, size_t lunghezza);          // il nome (non terminato) e un file di risorse conosciuto
bool        risFilePercorso(RisFile f, char* out, size_t cap);          // "/usr/share/vesevos/<nome>"

// Controlla l'elenco SHA256SUMS (formato e nomi conosciuti): all'avvio, con il risultato nel registro. RIS_OK, RIS_MANCA, RIS_FORMATO.
// L'esito si ricorda in RAM (risElencoStato).
RisEsito risElencoAvvio();
RisEsito risElencoStato();                   // ultimo esito noto; RIS_LETTURA finche non e stato controllato (dopo l'avvio o dopo risFileAzzera)
bool     risElencoNoto();                    // l'elenco e stato controllato almeno una volta dall'ultimo risFileAzzera

// Legge SHA256SUMS in buf (almeno RIS_ELENCO_MAX byte) e lo controlla (formato e nomi). n = byte letti. Aggiorna risElencoStato.
RisEsito risElencoLeggi(char* buf, size_t& n);
// Elenco buono e riga del file: hash = lo SHA-256 scritto per f. RIS_MANCA = il file non e nell'elenco (o l'elenco manca: vedi risElencoStato).
RisEsito risFileAtteso(RisFile f, uint8_t hash[32]);
size_t   risFileTetto(RisFile f);             // byte al massimo di quel file (html.vrs: intestazione + dati)
bool     risFileDaNome(const char* nome, RisFile& f);   // nome del file (come nell'elenco) -> RisFile

// Apre il file dopo averlo controllato (la prima volta per intero). vf resta aperto all'inizio, len = byte. Se non e RIS_OK vf e chiuso.
RisEsito risFileApri(RisFile f, VosFile& vf, size_t& len);

// Stato di ogni file, ricordato in RAM. E' UN SOLO posto: lo scrivono il controllo dopo l'avvio e la prima richiesta; lo leggono avvisi, pagina e shell.
RisStato risFileStato(RisFile f);
void     risFileImposta(RisFile f, RisStato s);          // chi controlla un file a pezzi (vos_risorse_avviso.cpp) scrive qui l'esito
// Apre il file senza controllarlo: c'e, non e una cartella, la dimensione e nei limiti. vf all'inizio; se non e RIS_OK vf e chiuso.
RisEsito risFileApriGrezzo(RisFile f, VosFile& vf, size_t& len);

// Dimentica i controlli fatti (dopo aver cambiato i file): uno o tutti. Chi controlla rifa' il controllo (vedi risFileGen).
void     risFileAzzera();
void     risFileAzzeraUno(RisFile f);
uint32_t risFileGen();                                   // cambia a ogni azzeramento: chi controlla a pezzi riparte da capo

