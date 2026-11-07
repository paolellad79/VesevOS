// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_eventi.h
// Eventi di sicurezza: gli ultimi errori di accesso, tenuti solo in RAM. Si azzerano al riavvio e non escono mai dalla scheda.
// Non si scrivono mai password, codici o nomi sbagliati: il nome c'e solo se l'utente esiste.
#pragma once
#include <stdint.h>

class HeapBuf;

#define EVENTI_VOCI 64

enum EventoTipo : uint8_t { EV_PASSWORD = 0, EV_MFA, EV_BLOCCO, EV_FILTRO, EV_RICHIESTE, EV_ALTRO, EV_TIPI };
enum EventoCanale : uint8_t { EVC_WEB = 0, EVC_SERIALE };

// Voci uguali arrivate entro 10 minuti diventano una voce sola con un contatore. ip = 0 se non c'e un indirizzo.
void eventoNota(EventoTipo tipo, EventoCanale canale, uint32_t ip, const char* nome = nullptr);
bool eventiJson(HeapBuf& j);                       // pagina Eventi di sicurezza (solo Admin)
void eventiSvuota();
uint32_t eventiBlocchiNuovi();                     // blocchi non ancora visti dall'amministratore: un solo numero per la campanella
void eventiBlocchiVisti();
