// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_i18n.h
// Traduzioni SEPARATE dal firmware. Il firmware contiene solo l'italiano: nel codice si scrive
// tr("testo italiano") e il testo italiano fa da chiave. Le altre lingue sono file nella memoria
// interna: /lang/<codice>.json (per esempio /lang/en.json), un oggetto JSON piatto
// {"testo italiano": "traduzione", ...}. La chiave speciale "_name" e il nome della lingua.
// Lo stesso file serve anche alla pagina web. Se manca la traduzione si usa l'italiano.
#pragma once
#include <Arduino.h>

void        langInit();                          // dopo LittleFS e cfgLoad: carica la lingua scelta
bool        langSet(const String& code, String& err);   // cambia lingua, salva, (ri)carica il file
bool        langCodeValid(const String& c);      // 2-8 lettere minuscole o numeri
String      langListJson();                      // [{"code":"it","name":"Italiano"},...]
const char* tr(const char* it);                  // testo nella lingua corrente
String      trf(const char* it, ...) __attribute__((format(printf, 1, 2)));   // come printf, con testo tradotto
