// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_led2.h
// Secondo LED della scheda: un LED a un solo colore con luminosita 0-255 (PWM, cfg.led2Brightness, come il LED1). Esiste solo se il file di scheda
// definisce VOS_PIN_LED2 (oggi la C6 SuperMini, GPIO15). Senza la macro tutte le funzioni sono vuote e led2Present() dice false.
// Ha gli stessi modi del LED1 (LedMode in core/vos_common.h, salvati in cfg.led2Mode), senza colori:
//   stato      battito (piu veloce con piu carico) = rete collegata, lampeggio lento = avvio, connessione o punto di accesso, lampo breve = modo aereo
//   heartbeat  battito a due impulsi, piu veloce con piu carico
//   fixed      acceso      none  spento
//   flash/wifi un lampo a ogni accesso alla flash o alla microSD / a ogni pacchetto Wi-Fi (sfondo spento)
//   cpu        acceso quando il carico supera la soglia cfg.led2CpuThr (sotto spento)
// La luminosita vale in tutti i modi (nel battito e il massimo). In piu, un errore grave fa lampeggiare veloce in ogni modo. I comandi manuali (shell, regole, funzioni VesevOS) valgono al posto del modo
// finche non si salva un modo nuovo o si dice "auto"; non salvano nulla.
#pragma once
#include <stdint.h>
#include "../core/vos_common.h"

enum Led2Mode : uint8_t { LED2_OFF = 0, LED2_ON = 1, LED2_BLINK = 2, LED2_AUTO = 3 };   // comando manuale; AUTO = vale il modo salvato

bool led2Present();                       // la scheda ha il LED2
void led2Init();                          // prenota il pin e lo mette spento (DOPO l'avvio: il pin di strapping non si forza prima)
void led2Shutdown();                      // spegne il pin (prima del sonno), senza cambiare nulla
bool led2Set(uint8_t mode);               // comando manuale; false se la scheda non ha il LED2 o il modo non vale
bool led2SetName(const char* w);          // "on" | "off" | "blink" | "auto" (anche 1, 0); false se il testo non vale
bool led2SetBrightness(int v);            // luminosita 0-255 salvata (tutti i modi); false se non c'e il LED2 o il valore non vale
bool led2Apply(const char* w, const char* arg);   // come led2SetName, ma "brightness" legge anche arg (0-255); per le regole
uint8_t led2Mode();                       // il comando manuale in corso (LED2_AUTO = nessuno)
const char* led2ModeName(uint8_t mode);   // testo inglese breve per la shell (on, off, blink, auto)
bool led2On();                            // il LED e acceso adesso (modo salvato o comando manuale)
void led2SetNetState(NetState s);         // lo stato di rete, per il modo "stato"
void led2SetFault(bool on);               // errore grave: lampeggio veloce
void led2SetSetup(bool on);               // prima configurazione in corso: respiro (solo senza LED RGB)
void led2Tick();                          // da chiamare di frequente (task del LED, circa ogni 20 ms)
