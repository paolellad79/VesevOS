// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_license.cpp
#include "vos_license.h"
#include "vos_license_data.h"   // GENERATO: LIC_DOCS[] e LIC_COUNT (incluso solo qui: un'unica copia in flash)

int         licCount() { return LIC_COUNT; }
const char* licId(int i)    { return (i >= 0 && i < LIC_COUNT) ? LIC_DOCS[i].id : ""; }
const char* licTitle(int i) { return (i >= 0 && i < LIC_COUNT) ? LIC_DOCS[i].title : ""; }
const char* licText(int i)  { return (i >= 0 && i < LIC_COUNT) ? LIC_DOCS[i].text : ""; }
size_t      licSize(int i)  { return (i >= 0 && i < LIC_COUNT) ? LIC_DOCS[i].size : 0; }

size_t      licZ(int i)     { return (i >= 0 && i < LIC_COUNT) ? LIC_DOCS[i].zsize : 0; }

int licFind(const String& id) {
  for (int i = 0; i < LIC_COUNT; i++) if (id.equalsIgnoreCase(LIC_DOCS[i].id)) return i;
  return -1;
}

String licListJson() {
  String j = "[";
  for (int i = 0; i < LIC_COUNT; i++) {
    if (i) j += ",";
    j += "{\"id\":\"" + String(LIC_DOCS[i].id) + "\",\"title\":\"" + String(LIC_DOCS[i].title) + "\",\"size\":" + String((unsigned)LIC_DOCS[i].size) + "}";
  }
  return j + "]";
}

String licIds() {
  String s;
  for (int i = 0; i < LIC_COUNT; i++) { if (i) s += ", "; s += LIC_DOCS[i].id; }
  return s;
}
