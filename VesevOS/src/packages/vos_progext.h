// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_progext.h
// STRATO 6 (packages): le estensioni dei programmi (script, app Berry, app ELF) con il loro nome vecchio. Il nome nuovo si scrive, il
// vecchio si legge ancora; se esistono tutti e due vale il nuovo. I nomi stanno in core/vos_ext.h (un solo punto).
#pragma once
#include <stddef.h>
#include <string_view>

struct ProgExt {
  const char* nuovo;       // ".vsb", ".vab", ".vae"
  const char* vecchio;     // ".vsc", ".vap", ".vel"
};

extern const ProgExt PEXT_SCRIPT;
extern const ProgExt PEXT_APP_B;
extern const ProgExt PEXT_APP_E;

// Quanti caratteri ha l'estensione (nuova o vecchia) alla fine di name; 0 se non finisce per nessuna delle due.
size_t progExtLen(std::string_view name, const ProgExt& e);

// true se name finisce per l'estensione nuova o per la vecchia.
bool progHaExt(const char* name, const ProgExt& e);

// Toglie dal nome l'estensione nuova o la vecchia (se c'e). Ritorna la nuova lunghezza di out.
size_t progTogliExt(char* out, size_t n, const ProgExt& e);

// Percorso del file del programma "nome" nella cartella dir: l'estensione nuova se il file c'e, altrimenti la vecchia se c'e quella,
// altrimenti la nuova (e il nome da usare per scrivere).
void progPercorso(char* out, size_t cap, const char* dir, const char* nome, const ProgExt& e);
// Come progPercorso, ma per LEGGERE e AVVIARE: se il programma non c'e in memoria interna prova la microSD ("/media/sd" + dir, per esempio
// /media/sd/bin). Vale la memoria interna se il nome c'e in tutte e due. Senza microSD montata il risultato e quello di progPercorso.
void progPercorsoAvvio(char* out, size_t cap, const char* dir, const char* nome, const ProgExt& e);

// Dopo aver scritto il file nuovo: toglie il vecchio omonimo, se c'e. Ritorna true se l'ha tolto.
bool progToglieVecchio(const char* dir, const char* nome, const ProgExt& e);

// Per un elenco: il file con il nome vecchio "nome<vecchio>" non si conta se esiste anche "nome<nuovo>" (vale il nuovo).
bool progVecchioCoperto(const char* dir, std::string_view nome, const ProgExt& e);
