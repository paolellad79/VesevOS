// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_aggiorna_pac.cpp
// App e driver installati con la versione dichiarata nel file. Il confronto con lo store lo fa la pagina.
#include <stdio.h>
#include <string.h>
#include <string_view>
#include "vos_aggiorna_int.h"
#include "vos_pacchetto_nome.h"
#include "../core/vos_ext.h"
#include "../core/vos_files.h"
#include "../core/vos_pacchetto.h"

namespace {

const int kMaxInstallati = 40;
const char* const kCartelle[] = {"/bin", "/sbin", "/devs"};

// Il tipo del file senza il punto; nullptr se non e un'app o un driver.
const char* tipoDi(std::string_view nome) {
  for (const char* ext : {VOS_EXT_APP_B, VOS_EXT_APP_E, VOS_EXT_DRV_B, VOS_EXT_DRV_E}) {
    size_t n = strlen(ext);
    if (nome.size() > n && nome.substr(nome.size() - n) == ext) return ext + 1;
  }
  return nullptr;
}

// Chiama fatto(nome corto, tipo, percorso) per ogni pacchetto. Si ferma a kMaxInstallati.
template <class F> void perOgniPacchetto(F fatto) {
  int contati = 0;
  for (const char* cartella : kCartelle) {
    HeapBuf j(4096);
    if (!fsListBuild(cartella, j)) continue;
    std::string_view v(j.c_str(), j.length());
    size_t pos = 0;
    while (contati < kMaxInstallati) {
      size_t a = v.find("{\"n\":\"", pos);
      if (a == v.npos) break;
      size_t b = v.find('"', a + 6);
      if (b == v.npos) break;
      std::string_view nome = v.substr(a + 6, b - (a + 6));
      pos = b + 1;
      const char* tipo = tipoDi(nome);
      if (!tipo) continue;
      char corto[25], percorso[FS_PATH_MAX + 1];
      snprintf(corto, sizeof corto, "%.*s", (int)(nome.size() - strlen(tipo) - 1), nome.data());
      snprintf(percorso, sizeof percorso, "%s/%.*s", cartella, (int)nome.size(), nome.data());
      fatto(corto, tipo, percorso);
      contati++;
    }
  }
}

}  // namespace

bool aggPacchettiJson(HeapBuf& j) {
  j.add("{\"ok\":true,\"lista\":[");
  bool primo = true;
  perOgniPacchetto([&](const char* corto, const char* tipo, const char* percorso) {
    PacDati d;
    pacchettoLeggiDati(percorso, d);
    j.addf("%s{\"n\":\"", primo ? "" : ",");
    j.addJson(corto);
    j.addf("\",\"e\":\"%s\",\"v\":\"", tipo);
    j.addJson(d.versione);
    j.add("\"}");
    primo = false;
  });
  return j.add("]}");
}
