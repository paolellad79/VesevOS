// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_led_nomi.h
// Nomi dei LED (decisione dell'utente del 13/11/2026): il nome vero e classe + numero con trattino basso.
// Classe "led_rgb" (a colori): led_rgb1, led_rgb2...  Classe "led" (monocromatici): led1, led2...  Ogni classe ha la sua numerazione,
// uguale su tutte le schede. I nomi vecchi ("led" = LED a colori, "led2" = secondo LED) non valgono piu: danno un errore che dice il nome nuovo.
#pragma once
#include "../core/vos_common.h"

// Classi di funzione della tabella di scheda (VOS_BOARD_PERIF): LEDRGB e a colori, LEDONOFF e monocromatico, TASTO non e un LED.
#define LEDCLS_LEDRGB   1
#define LEDCLS_LEDONOFF 2
#define LEDCLS_TASTO    0
#define LEDNOMI_CONTA_RGB(id, etichetta, colore, gruppo, tab, funzione, widget, pin, nota)  + (LEDCLS_##funzione == LEDCLS_LEDRGB)
#define LEDNOMI_CONTA_MONO(id, etichetta, colore, gruppo, tab, funzione, widget, pin, nota) + (LEDCLS_##funzione == LEDCLS_LEDONOFF)

// Quanti LED di bordo ha la scheda, per classe (contati in compilazione dalla tabella)
constexpr int kLedBordoRgb  = 0 VOS_BOARD_PERIF(LEDNOMI_CONTA_RGB);
constexpr int kLedBordoMono = 0 VOS_BOARD_PERIF(LEDNOMI_CONTA_MONO);

// Numero del primo LED NUOVO di una classe = LED di bordo della classe + 1 (S3: led1; C6 e C3: led2). Classi: "led", "led_rgb"; altre: 1.
int ledNumeroIniziale(const char* classe);

// Nome vecchio ("led", "led2") usato al posto del nuovo: il messaggio di errore con il nome nuovo; nullptr = non e un nome vecchio.
const char* ledNomeVecchioMsg(const char* nome);
