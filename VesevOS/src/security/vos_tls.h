// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_tls.h
// Certificato HTTPS unico per ogni scheda: chiave ECDSA P-256 creata sulla scheda (mai uguale a un'altra),
// certificato autofirmato valido 10 anni per <nome>.local, <nome>, 192.168.4.1 e l'IP attuale.
// L'impronta (SHA-256) si mostra sulla seriale e nella pagina: l'utente la confronta con quella del browser.
// In alternativa si puo caricare un proprio certificato (PEM) con la sua chiave.
#pragma once
#include "../core/vos_errbuf.h"
#include "../core/vos_heapbuf.h"

#define TLS_FP_BUF 96                  // impronta "AB:CD:..." (32 byte = 95 caratteri) + zero
#define TLS_JSON_MAX 512               // tetto del JSON di tlsJson

bool   tlsEnsure();                    // crea il certificato se manca o se il nome e cambiato (1-2 s); true se pronto
const char* tlsCertPem();              // "" se non c'e ancora; il testo resta valido finche la scheda e accesa (non cambia mentre il server lo usa)
const char* tlsKeyPem();
const char* tlsFingerprint();          // "AB:CD:..." (SHA-256 del certificato), "" se non c'e
bool   tlsCustom();                    // certificato caricato dall'utente
bool   tlsSetCustom(const char* certPem, const char* keyPem, ErrBuf& err);   // vale dal prossimo riavvio
void   tlsRegenerate();                // nuovo certificato autofirmato (vale dal prossimo riavvio)
bool   tlsPending();                   // c'e un certificato nuovo che aspetta il riavvio
void   tlsJson(HeapBuf& j);
