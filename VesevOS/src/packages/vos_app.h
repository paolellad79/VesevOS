// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_app.h
// App (.vap): UN file di testo in /apps con manifest, icona, pagina (controlli dichiarati), codice Berry e dati iniziali.
// 1.7.52: lettura e controllo del file. 1.7.53: ESECUZIONE (start/stop/set), pagina generica con i controlli dichiarati.
// Formato: claude/funzioni/VesevOS-formato-vap.md
//   [manifest]  name= title= version= author= use= who=
//   [icon]      un solo <svg> (max 2 KB, solo forme semplici: niente script, collegamenti, immagini, stili)
//   [page]      una riga per controllo:  switch|slider|color|text|button|chart id=... label="..." [min= max= points=]
//   [code]      codice Berry (max 8 KB) fino alla prossima sezione
//   [data]      chiave=valore (valori iniziali)
#pragma once
#include <Arduino.h>
#include "../core/vos_common.h"

#if VOS_WITH_BERRY
#define APP_DIR        "/apps"
#define APP_EXT        ".vap"
#define APP_FILE_MAX   16384
#define APP_ICON_MAX   2048
#define APP_CODE_MAX   8192
#define APP_CTL_MAX    12
#define APP_DATA_MAX   12

struct AppCtl { char k; char id[13]; char label[25]; int32_t mn, mx; uint16_t pts; };   // k: s=switch l=slider c=color t=text b=button g=chart
struct AppInfo {
  String name, title, ver, author, who, use, icon, code, data;   // code = testo [code]; data = righe chiave=valore di [data]
  AppCtl ctl[APP_CTL_MAX]; int nctl;
  int ndata; size_t size, codeLen;
};

// Legge e controlla il testo di un file .vap. fileName = nome senza cartella ne estensione (deve essere uguale a name=).
// true = valido (a riempito). false = err contiene "Riga N: motivo".
bool   appParse(const String& text, const String& fileName, AppInfo& a, String& err);
bool   appIconSafe(const String& svg);          // true se l'icona SVG e ammessa (solo forme semplici)
bool   appNameValid(const String& name);
String appListText();                           // comando "app list"
String appInfoText(const String& name);         // comando "app info <nome>"
String appListJson(uint8_t role = 2);             // GET /api/apps (operatore: solo le app valide con who=oper)
String appInfoJson(const String& name, uint8_t role = 2);   // GET /api/app?name= (operatore: solo se who=oper)
// ---- esecuzione (1.7.53): una sola app alla volta, nello stesso motore Berry degli script (si escludono a vicenda) ----
// role: 1 = operatore, 2 = admin. L'operatore vede e usa solo le app con who=oper.
bool   appStart(const String& name, String& err, uint8_t role, const char* who);
bool   appStop(const String& name, String& err, uint8_t role);
bool   appSet(const String& name, const String& id, const String& value, String& err, uint8_t role);   // un controllo della pagina -> on_set(id, valore)
String appStateJson(const String& name, uint8_t role);   // GET /api/app/state: in esecuzione?, valori dei controlli, uscita, errore
String appRunText();                                     // comando "app out"
#else
inline String appListText() { return "App: non presenti in questo firmware\n"; }
inline String appInfoText(const String&) { return "App: non presenti in questo firmware\n"; }
inline String appListJson(uint8_t = 2) { return "{\"ok\":false,\"err\":\"App non presenti in questo firmware\"}"; }
inline String appInfoJson(const String&, uint8_t = 2) { return "{\"ok\":false,\"err\":\"App non presenti in questo firmware\"}"; }
inline bool   appStart(const String&, String& err, uint8_t, const char*) { err = "App non presenti in questo firmware"; return false; }
inline bool   appStop(const String&, String& err, uint8_t) { err = "App non presenti in questo firmware"; return false; }
inline bool   appSet(const String&, const String&, const String&, String& err, uint8_t) { err = "App non presenti in questo firmware"; return false; }
inline String appStateJson(const String&, uint8_t) { return "{\"ok\":false,\"err\":\"App non presenti in questo firmware\"}"; }
inline String appRunText() { return "App: non presenti in questo firmware\n"; }
#endif
