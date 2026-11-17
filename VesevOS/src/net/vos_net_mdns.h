// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_net_mdns.h
// Annuncio del nome in rete (mDNS) del task di rete. Se l'avvio fallisce si scrive il motivo e si riprova poche volte.
#pragma once

void netMdnsStop();      // PRIMA di spegnere o rifare la radio: mDNS tiene i puntatori alle interfacce di rete
void netMdnsApply();     // (ri)avvia secondo le impostazioni; se non parte, programma i nuovi tentativi
void netMdnsRiprova();   // un passo al secondo, dal task di rete: esegue un nuovo tentativo se e l'ora
bool netMdnsRun();       // sta annunciando il nome?
