// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_app.h
// App (.vap): UN file di testo in /apps con manifest, icona, pagina (controlli dichiarati), codice Berry e dati iniziali.
// 1.7.52: lettura e controllo del file. 1.7.53: ESECUZIONE (start/stop/set), pagina generica con i controlli dichiarati.
// Formato: vedi il manuale (app .vap)
//   [manifest]  name= title= version= author= use= who=
//   [icon]      un solo <svg> (max 2 KB, solo forme semplici: niente script, collegamenti, immagini, stili)
//   [page]      una riga per controllo:  switch|slider|color|text|button|chart id=... label="..." [min= max= points=]
//   [code]      codice Berry (max 8 KB) fino alla prossima sezione
//   [data]      chiave=valore (valori iniziali)
#pragma once
#include <string_view>
#include "../core/vos_errbuf.h"
#include "../core/vos_heapbuf.h"
#include "../core/vos_common.h"
#include "vos_berry.h"

// Tetti dei testi e dei JSON (oltre: errore, mai testo troncato in silenzio)
#define APP_TEXT_MAX   4096     // testi della shell (app list, app info)
#define APP_INFO_MAX   16384    // JSON di una app (icona fino a 2 KB, 12 controlli)
#define APP_LIST_MAX   65536    // JSON dell'elenco (fino a 12 app con icona)
#define APP_STATE_MAX  45056    // JSON dello stato: uscita (BERRY_STATE_MAX) + valori dei controlli e dei grafici

#if VOS_WITH_BERRY
#define APP_DIR        "/apps"
#define APP_EXT        ".vap"
#define APP_FILE_MAX   16384
#define APP_ICON_MAX   2048
#define APP_CODE_MAX   8192
#define APP_CTL_MAX    12
#define APP_DATA_MAX   12

struct AppCtl { char k; char id[13]; char label[25]; int32_t mn, mx; uint16_t pts; };   // k: s=switch l=slider c=color t=text b=button g=chart
// Tutto a buffer fissi (l'oggetto sta sempre sull'heap: ~11 KB). icon e code sono chiusi con 0; data = righe chiave=valore di [data].
struct AppInfo {
  char name[25], title[25], ver[11], author[25], who[6], use[100];
  char icon[APP_ICON_MAX + 16];
  char code[APP_CODE_MAX + 16];
  char data[APP_DATA_MAX * 54 + 8];
  AppCtl ctl[APP_CTL_MAX]; int nctl;
  int ndata; size_t size, codeLen;
};

// Legge e controlla il testo di un file .vap. fileName = nome senza cartella ne estensione (deve essere uguale a name=).
// true = valido (a riempito). false = err contiene "Riga N: motivo".
bool   appParse(std::string_view text, const char* fileName, AppInfo& a, ErrBuf& err);
bool   appIconSafe(std::string_view svg);       // true se l'icona SVG e ammessa (solo forme semplici)
bool   appNameValid(std::string_view name);
// Testi e JSON si scrivono in out (con il suo tetto). I testi non danno errore; i JSON ritornano false se manca la memoria o si supera il tetto (out da buttare).
void   appListText(HeapBuf& out);                                  // comando "app list"
void   appInfoText(const char* name, HeapBuf& out);                // comando "app info <nome>"
bool   appListJson(HeapBuf& out, uint8_t role = 2);                // GET /api/apps (operatore: solo le app valide con who=oper)
bool   appInfoJson(const char* name, HeapBuf& out, uint8_t role = 2);   // GET /api/app?name= (operatore: solo se who=oper)
// ---- esecuzione (1.7.53): una sola app alla volta, nello stesso motore Berry degli script (si escludono a vicenda) ----
// role: 1 = operatore, 2 = admin. L'operatore vede e usa solo le app con who=oper.
bool   appStart(const char* name, ErrBuf& err, uint8_t role, const char* who);
bool   appStop(const char* name, ErrBuf& err, uint8_t role);
bool   appSet(const char* name, const char* id, const char* value, ErrBuf& err, uint8_t role);   // un controllo della pagina -> on_set(id, valore)
bool   appStateJson(const char* name, HeapBuf& out, uint8_t role);   // GET /api/app/state: in esecuzione?, valori dei controlli, uscita, errore
void   appRunText(HeapBuf& out);                                     // comando "app out"
#else
inline void   appListText(HeapBuf& o) { o.add("App: non presenti in questo firmware\n"); }
inline void   appInfoText(const char*, HeapBuf& o) { o.add("App: non presenti in questo firmware\n"); }
inline bool   appListJson(HeapBuf& o, uint8_t = 2) { return o.add("{\"ok\":false,\"err\":\"App non presenti in questo firmware\"}"); }
inline bool   appInfoJson(const char*, HeapBuf& o, uint8_t = 2) { return o.add("{\"ok\":false,\"err\":\"App non presenti in questo firmware\"}"); }
inline bool   appStart(const char*, ErrBuf& err, uint8_t, const char*) { err = "App non presenti in questo firmware"; return false; }
inline bool   appStop(const char*, ErrBuf& err, uint8_t) { err = "App non presenti in questo firmware"; return false; }
inline bool   appSet(const char*, const char*, const char*, ErrBuf& err, uint8_t) { err = "App non presenti in questo firmware"; return false; }
inline bool   appStateJson(const char*, HeapBuf& o, uint8_t) { return o.add("{\"ok\":false,\"err\":\"App non presenti in questo firmware\"}"); }
inline void   appRunText(HeapBuf& o) { o.add("App: non presenti in questo firmware\n"); }
#endif
