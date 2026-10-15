// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_btn.h
// Click del tasto BOOT: 1, 2 o 3 click veloci, ognuno con l'azione scelta dalla pagina (Dispositivi > Tasto BOOT).
// Le pressioni lunghe (2 s, 8 s, 20 s) restano fisse e stanno nel file principale: servono al recupero.
#pragma once
#include <Arduino.h>
#include "../core/vos_common.h"

enum BtnAct : uint8_t { BA_NONE = 0, BA_AIR_OFF = 1, BA_WPS = 2, BA_FIND = 3, BA_MAX = 4 };
#define BTN_CLICKS 3            // click gestiti
#define BTN_GAP_MS 500          // dopo l'ultimo rilascio, tanto silenzio = il gesto e finito

void   btnClick();              // un click breve (tasto lasciato dopo meno di 2 s)
void   btnLongRelease();        // tasto lasciato dopo 2 s o piu: i click in sospeso si perdono
void   btnTick(bool down);      // dal loop: fa partire il gesto finito
uint8_t btnAction(int clicks);  // azione scelta per 1..3 click
bool   btnSetAction(int clicks, int act, String& err);
int    btnWpsClicks();          // con quanti click parte il WPS (0 = nessuno)
const char* btnActName(int act);
// Click LIBERI: un gesto con l'azione "Libero" non fa niente di suo; se un programma (script o app Berry) e in esecuzione e li chiede
// con btn(), i click vanno a lui. Senza programma in esecuzione si perdono (niente click vecchi che arrivano dopo).
void   btnScriptAttach(bool on);       // dal task Berry: un programma prende (o lascia) i click liberi
int    btnScriptPop();                 // il click libero piu vecchio (1..3), 0 = nessuno. Non aspetta.
bool   btnScriptFree(int clicks);      // questo gesto e libero per i programmi?

// Modo prova (dalla pagina, 60 s al massimo): il tasto non fa NIENTE, nemmeno le pressioni lunghe; la pagina mostra cosa e stato capito.
#define BTN_TEST_MS 60000
void   btnTestSet(bool on);
bool   btnTestActive();
void   btnTestLong(uint32_t heldMs);   // tasto lasciato dopo 2 s o piu in modo prova: solo annotato
String btnJson();
String btnText();
