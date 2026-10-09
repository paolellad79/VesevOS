// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_i18n.h
// Traduzioni. Nel codice si scrive tr("testo italiano") e il testo italiano fa da chiave.
// Italiano e inglese sono DENTRO il firmware (l'inglese e generato da tools/mklang.py in vos_lang_en.h).
// Le altre lingue sono file nella memoria interna: /lang/<codice>.json, un oggetto JSON piatto
// {"testo italiano": "traduzione", ...}. Chiavi speciali: "_name" nome della lingua, "_locale" (es. "es-ES"),
// "_flag" bandiera in SVG (piccola, disegnata a mano). Lo stesso file serve anche alla pagina web.
#pragma once
#include <Arduino.h>

void        langInit();                          // dopo LittleFS e cfgLoad: carica la lingua scelta
bool        langSet(const String& code, String& err);   // cambia lingua, salva, (ri)carica il file
bool        langCodeValid(const String& c);      // 2-8 lettere minuscole o numeri
String      langListJson();                      // [{"code":"it","name":"Italiano","loc":"it-IT","flag":"<svg..>"},...]
int         langList(String* codes, String* names, int max);   // lingue disponibili (per il menu della seriale)
bool        langBuiltin(const String& code);     // "it" o "en"
const uint8_t* langBuiltinJson(const String& code, size_t& len);   // file della lingua interna, compresso gzip (per la pagina)
const char* tr(const char* it);                  // testo nella lingua corrente
String      trf(const char* it, ...) __attribute__((format(printf, 1, 2)));   // come printf, con testo tradotto
