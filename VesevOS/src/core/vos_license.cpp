// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_license.cpp
#include "vos_license.h"
#include <strings.h>
#include "vos_strbuf.h"
#include "vos_license_data.h"   // GENERATO: LIC_DOCS[] e LIC_COUNT (incluso solo qui: un'unica copia in flash)

int         licCount() { return LIC_COUNT; }
const char* licId(int i)    { return (i >= 0 && i < LIC_COUNT) ? LIC_DOCS[i].id : ""; }
const char* licTitle(int i) { return (i >= 0 && i < LIC_COUNT) ? LIC_DOCS[i].title : ""; }
const char* licText(int i)  { return (i >= 0 && i < LIC_COUNT) ? LIC_DOCS[i].text : ""; }
size_t      licSize(int i)  { return (i >= 0 && i < LIC_COUNT) ? LIC_DOCS[i].size : 0; }

size_t      licZ(int i)     { return (i >= 0 && i < LIC_COUNT) ? LIC_DOCS[i].zsize : 0; }

int licFind(const char* id) {
  if (!id) return -1;
  for (int i = 0; i < LIC_COUNT; i++) if (strcasecmp(id, LIC_DOCS[i].id) == 0) return i;
  return -1;
}

size_t licListJson(char* out, size_t cap) {
  StrBuf j(out, cap);
  j.add('[');
  for (int i = 0; i < LIC_COUNT; i++) {
    if (i) j.add(',');
    j.add("{\"id\":\"").add(LIC_DOCS[i].id).add("\",\"title\":\"").add(LIC_DOCS[i].title);
    j.add("\",\"size\":").addu((uint32_t)LIC_DOCS[i].size).add('}');
  }
  j.add(']');
  return j.length();
}

size_t licIds(char* out, size_t cap) {
  StrBuf s(out, cap);
  for (int i = 0; i < LIC_COUNT; i++) { if (i) s.add(", "); s.add(LIC_DOCS[i].id); }
  return s.length();
}
