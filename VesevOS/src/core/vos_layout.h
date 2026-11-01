// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_layout.h
// Struttura delle cartelle della memoria interna (stile Debian). Tempo 1: la configurazione
//   /vesevos.conf  ->  /etc/vesevos.conf   (con .bak e .tmp accanto, come prima)
// Passo B (migrazione completa), stessa regola di copia verificata:
//   /apps -> /bin   /scripts -> /sbin   /backup -> /var/backup   /audit.log /crash.txt /boots3.bin -> /var/...
// Restano dove sono: /lang /rules.txt /io.cfg /mqtt-ca.pem /updurl.txt.
// Marcatore nella NVS (vos/layout): 0 = niente, 1 = configurazione copiata, 2 = anche il resto copiato (da qui i percorsi nuovi sono
// quelli in uso), 3 = vecchi file tolti (si fa all'avvio DOPO quello della copia, quando l'avvio e riuscito: layoutFinish).
// Usa solo drivers/ e core/ (nessuna chiamata verso l'alto).
#pragma once
#include <stddef.h>
#include <stdint.h>

#define VOS_CONF_FILE "/etc/vesevos.conf"
#define VOS_CONF_TMP  "/etc/vesevos.conf.tmp"
#define VOS_CONF_BAK  "/etc/vesevos.conf.bak"

// Posto vecchio della configurazione: letto se quello nuovo manca.
#define VOS_CONF_OLD     "/vesevos.conf"
#define VOS_CONF_OLD_BAK "/vesevos.conf.bak"
#define VOS_CONF_OLD_TMP "/vesevos.conf.tmp"

// 1 = a ogni salvataggio si aggiorna anche /vesevos.conf, perche il recovery vecchio (fino alla 0.5.1) legge solo quello.
// Quando il recovery 0.5.2 e su tutte le schede si mette 0: il vecchio file smette di essere aggiornato.
#define VOS_CONF_MIRROR_OLD 1

// Crea (vuote, se mancano) /etc /var /tmp /home /bin /sbin. Una cartella che c'e gia va bene.
void layoutEnsureDirs();

// Al primo avvio con questa struttura copia /vesevos.conf in /etc/vesevos.conf (il vecchio resta). Da chiamare dopo drvFsBegin() e
// PRIMA di cfgLoad(). Ripetibile senza danni: il marcatore nella NVS si scrive solo a copia verificata.
// Ritorna una riga per il registro (copia fatta, oppure avviso se non e riuscita), nullptr se non c'e niente da dire. Non scrive nel
// registro da sola: cosi questo modulo non entra nel ciclo di include del nucleo (checklayers, famiglia "cicli").
const char* layoutInit();

// Scrive n byte in path in modo sicuro: prima "path.tmp~", poi controlla la dimensione, poi rename (sostituisce il file in un passo).
// Se manca la corrente a meta il file vecchio resta intero. false se non riesce (il temporaneo viene tolto).
bool layoutWriteAtomic(const char* path, const uint8_t* data, size_t n);

// ---- Passo B: percorsi in uso ----
enum LayoutItem { LAY_APPS, LAY_SCRIPTS, LAY_BACKUP, LAY_AUDIT, LAY_CRASH, LAY_DIARY, LAY_COUNT };

// Il percorso da usare ora: quello nuovo se il marcatore e almeno 2, altrimenti il vecchio (copia non ancora fatta o non riuscita).
// Funziona anche prima di layoutInit() (allora vale il vecchio).
const char* layoutPath(LayoutItem it);
// Il percorso nuovo e quello vecchio di una voce (per le scorciatoie dei nomi, vedi vos_paths.h).
const char* layoutPathNew(LayoutItem it);
const char* layoutPathOld(LayoutItem it);
// Marcatore corrente (0-3).
int layoutState();

// Da chiamare quando l'avvio e riuscito (vos_boot: bootStable, dopo 60 s). Se il marcatore era gia 2 AL PRINCIPIO di questo avvio
// toglie i vecchi file e porta il marcatore a 3. Altrimenti non fa niente. Ripetibile: se qualcosa non si cancella si riprova al prossimo avvio.
// Ritorna una riga per il registro oppure nullptr.
const char* layoutFinish();
