// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_risorse_tocco.h
// Avviso in audit quando qualcuno scrive, rinomina, copia, toglie o estrae un file dentro /usr/share/vesevos con le funzioni dei file
// (pagina, shell, estrazione di archivi...). Solo un avviso: nessun blocco (decisione dell'utente del 16/11/2026, progetto open source).
// Il punto comune e il gancio di drivers/vos_drv_fs (ogni scrittura vera passa da li). Le scritture della nostra rotta di caricamento
// (/api/risorse/carica, file con lo SHA-256 giusto) NON sono manomissioni: danno solo una riga informativa.
#pragma once

void risToccoInit();                             // registra il gancio (una volta, dopo il montaggio del file system)

// Dice chi sta agendo in questo task (nome dell'utente o "shell"): lo chiamano la pagina (a ogni richiesta con sessione) e la shell.
// L'elenco e piccolo (4 task): se i task sono di piu, il piu vecchio perde il nome e diventa "sistema". Mai password o altro.
void risAttore(const char* chi);

// Dentro questo oggetto le scritture sono della nostra rotta di caricamento: nessun avviso di manomissione.
class RisToccoNostro {
 public:
  RisToccoNostro();
  ~RisToccoNostro();
  RisToccoNostro(const RisToccoNostro&) = delete;
  RisToccoNostro& operator=(const RisToccoNostro&) = delete;
};

// Riga informativa (livello INFO) per un file caricato dalla nostra rotta: "file di risorse <nome> sostituito da <chi>".
void risToccoCaricato(const char* nome);
