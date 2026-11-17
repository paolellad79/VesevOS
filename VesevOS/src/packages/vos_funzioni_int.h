// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_funzioni_int.h: stato interno delle funzioni VesevOS comune a vos_funzioni.cpp e vos_funzioni_scripts.cpp (non usare altrove).
#pragma once
#include "vos_funzioni.h"
#include "../core/vos_layout.h"
#include "../core/vos_ext.h"
#include "vos_progext.h"
#include <freertos/FreeRTOS.h>   // portMUX_TYPE

#define FUNZIONI_CODA_MAX    4      // script in coda (azione "script <nome>" delle Automazioni)
#define SCRIPT_DIR    layoutPath(LAY_SCRIPTS)   // /sbin (prima della copia /scripts, vedi core/vos_layout.h)
#define SCRIPT_EXT    VOS_EXT_SCRIPT      // da core/vos_ext.h
#define FUNZIONI_ERR_BUF 321    // messaggio d'errore dell'ultima esecuzione

namespace vosfunzioni {
extern portMUX_TYPE g_mux;
extern volatile bool g_busy, g_bg, g_stop, g_app, g_admin, g_appAdmin, g_cmd;   // g_admin: lanciato dall'Admin (script run, pagina Script, funzioni)
extern char     g_obuf[FUNZIONI_USCITA_MAX + 1]; extern size_t g_olen; extern bool g_otrunc;
extern char     g_ebuf[FUNZIONI_ERR_BUF];     extern size_t g_elen;
extern char     g_name[25];
extern uint32_t g_startMs;      // quando e partita l'esecuzione in corso
extern uint32_t g_runs, g_lastMs, g_lastPeak, g_lastLeak; extern bool g_lastOk;
extern int g_qn;
extern volatile bool g_toErr;
extern size_t g_used, g_peak;

void qClear();
size_t funzioniLiberaTutto();         // in vos_funzioni_mem.cpp: libera tutto cio che lo script ha lasciato
bool useHas(const char* w);     // permesso presente nel use= dell'app in corso?
int  migrateOld();              // sposta i vecchi script .be di /apps in /scripts (una volta per avvio)
}
using namespace vosfunzioni;
struct bvm;
// in vos_funzioni_ble.cpp (funzioni Bluetooth, vedi li): app = app con use=ble (tutte le funzioni), altrimenti solo gestione (admin = lanciato dall'Admin)
void funzioniRegistraBle(bvm* vm, bool app, bool admin);
int  funzioniBleSmista(bvm* vm, bool& did);   // eventi del telefono -> on_write / on_read dell'app; 0 = bene, altro = errore funzioni VesevOS
void funzioniBleTerminato(const char* appName);     // l'app e finita: il Bluetooth si spegne
void funzioniBleInit();                         // una volta per esecuzione: collega la rete di sicurezza
// in vos_funzioni_files.cpp: listdir() e stat() (solo lettura dei metadati). Gli script le hanno sempre; le app solo con "files" in use=.
void funzioniRegistraFile(bvm* vm);
// aiuti di vos_funzioni_files.cpp: scrivono chiave -> valore nella mappa che sta in cima alla pila
void funzioniMettiInt(bvm* vm, const char* k, long v);
void funzioniMettiBool(bvm* vm, const char* k, bool v);
void funzioniMettiTesto(bvm* vm, const char* k, const char* v);
void funzioniAvvolgi(bvm* vm, const char* cls);
void funzioniRichiedeFunz(bvm* vm, const char* group);   // se l'utente ha tolto la funzionalita (Sistema > Funzionalita): eccezione "funzionalita spenta"; si chiama all'inizio delle funzioni sd_*, gpio_*, i2c_*, spi_*, dev_*   // mappa/lista grezza in cima -> oggetto "map"/"list" usabile dallo script
// in vos_funzioni_data.cpp: data_dir(), data_write(), data_read(), data_rm() della cartella dei dati; solo le app con data=1 e "files" in use=
void funzioniRegistraDati(bvm* vm, const char* appName);
// in vos_funzioni_dev.cpp: dev_get() e dev_set() delle app
void funzioniRegistraDev(bvm* vm);
// in vos_funzioni_dev.cpp: pwm(nome, duty [, hz]); la funzione si registra solo con VOS_WITH_PWM (altrimenti non fa nulla). app = app (il nome serve in use=); admin = puo cambiare la frequenza
void funzioniRegistraPwm(bvm* vm, bool app, bool admin);
// in vos_funzioni_dev.cpp: contatore(nome [, azzera]); si registra solo con VOS_WITH_CONTATORE (altrimenti non fa nulla)
void funzioniRegistraContatore(bvm* vm, bool app);
// in vos_funzioni_https.cpp: scarica(url, dimMax) = testo di un file https (driver vos_drv_https). Script sempre; app solo con https in use=.
void funzioniRegistraHttps(bvm* vm);
// in vos_funzioni_sd.cpp: funzioni della microSD (vedi li). app = app con use=sd; admin = script lanciato dall'Admin o app con who=admin.
void funzioniRegistraSd(bvm* vm, bool app, bool admin);
// in vos_funzioni_hw.cpp: gpio_*, i2c_*, spi_transfer, uptime, delay_us dei driver (solo con i moduli ELF). app = solo i gruppi in use=; admin = scrittura consentita.
void funzioniRegistraHw(bvm* vm, bool app, bool admin);
void funzioniRegistraLed(bvm* vm, bool rgb, bool mono, bool vecchioLed, bool vecchioLed2);   // vos_funzioni_led.cpp: led_rgb1(), led1() e i nomi vecchi (errore chiaro)
// in vos_funzioni_cmd.cpp: comandi dei driver. cmdCallSet copia gli argomenti (si chiama quando il "busy" e gia nostro); funzioniRegistraCmd da' al codice args() e cmd().
void cmdCallSet(const CmdCall& call);
void funzioniRegistraCmd(bvm* vm);
