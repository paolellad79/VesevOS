// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_berry_int.h: stato interno di Berry comune a vos_berry.cpp e vos_berry_scripts.cpp (non usare altrove).
#pragma once
#include "vos_berry.h"
#include "../core/vos_layout.h"
#include "../core/vos_ext.h"
#include "vos_progext.h"
#include <freertos/FreeRTOS.h>   // portMUX_TYPE

#define BERRY_QMAX    4      // script in coda (azione "script <nome>" delle Automazioni)
#define SCRIPT_DIR    layoutPath(LAY_SCRIPTS)   // /sbin (prima della copia /scripts, vedi core/vos_layout.h)
#define SCRIPT_EXT    VOS_EXT_SCRIPT      // da core/vos_ext.h
#define BERRY_EBUF_SZ 321    // messaggio d'errore dell'ultima esecuzione

namespace vosberry {
extern portMUX_TYPE g_mux;
extern volatile bool g_busy, g_bg, g_stop, g_app, g_admin, g_appAdmin, g_cmd;   // g_admin: lanciato dall'Admin (script run, pagina Script, berry)
extern char     g_obuf[BERRY_OUT_MAX + 1]; extern size_t g_olen; extern bool g_otrunc;
extern char     g_ebuf[BERRY_EBUF_SZ];     extern size_t g_elen;
extern char     g_name[25];
extern uint32_t g_startMs;      // quando e partita l'esecuzione in corso
extern uint32_t g_runs, g_lastMs, g_lastPeak, g_lastLeak; extern bool g_lastOk;
extern int g_qn;
extern volatile bool g_toErr;
extern size_t g_used, g_peak;

void qClear();
size_t berryFreeAll();         // in vos_berry_mem.cpp: libera tutto cio che lo script ha lasciato
bool useHas(const char* w);     // permesso presente nel use= dell'app in corso?
int  migrateOld();              // sposta i vecchi script .be di /apps in /scripts (una volta per avvio)
}
using namespace vosberry;
struct bvm;
// in vos_berry_ble.cpp (funzioni Bluetooth, vedi li): app = app con use=ble (tutte le funzioni), altrimenti solo gestione (admin = lanciato dall'Admin)
void berryBleRegister(bvm* vm, bool app, bool admin);
int  berryBleDispatch(bvm* vm, bool& did);   // eventi del telefono -> on_write / on_read dell'app; 0 = bene, altro = errore Berry
void berryBleEnded(const char* appName);     // l'app e finita: il Bluetooth si spegne
void berryBleInit();                         // una volta per esecuzione: collega la rete di sicurezza
// in vos_berry_files.cpp: listdir() e stat() (solo lettura dei metadati). Gli script le hanno sempre; le app solo con "files" in use=.
void berryFilesRegister(bvm* vm);
// aiuti di vos_berry_files.cpp: scrivono chiave -> valore nella mappa che sta in cima alla pila
void berryPutInt(bvm* vm, const char* k, long v);
void berryPutBool(bvm* vm, const char* k, bool v);
void berryPutText(bvm* vm, const char* k, const char* v);
void berryWrap(bvm* vm, const char* cls);
void berryNeedFunc(bvm* vm, const char* group);   // se l'utente ha tolto la funzionalita (Sistema > Funzionalita): eccezione "funzionalita spenta"; si chiama all'inizio delle funzioni sd_*, gpio_*, i2c_*, spi_*, dev_*   // mappa/lista grezza in cima -> oggetto "map"/"list" usabile dallo script
// in vos_berry_data.cpp: data_dir(), data_write(), data_read(), data_rm() della cartella dei dati; solo le app con data=1 e "files" in use=
void berryDataRegister(bvm* vm, const char* appName);
// in vos_berry_dev.cpp: dev_get() e dev_set() delle app
void berryDevRegister(bvm* vm);
// in vos_berry_sd.cpp: funzioni della microSD (vedi li). app = app con use=sd; admin = script lanciato dall'Admin o app con who=admin.
void berrySdRegister(bvm* vm, bool app, bool admin);
// in vos_berry_hw.cpp: gpio_*, i2c_*, spi_transfer, uptime, delay_us dei driver (solo con i moduli ELF). app = solo i gruppi in use=; admin = scrittura consentita.
void berryHwRegister(bvm* vm, bool app, bool admin);
// in vos_berry_cmd.cpp: comandi dei driver. cmdCallSet copia gli argomenti (si chiama quando il "busy" e gia nostro); berryCmdRegister da' al codice args() e cmd().
void cmdCallSet(const CmdCall& call);
void berryCmdRegister(bvm* vm);
