// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_elf.cpp
// Percorsi del server web per i moduli ELF (solo con VOS_WITH_ELF): elenco dei moduli installati in /bin con il controllo del file e lo
// stato, moduli in corso, avvio, stop e quarantena. L'interruttore del servizio e /api/svc (vos_web_net.cpp). Il caricamento di un
// .vae e il normale caricamento file (/api/fs/up?dir=/bin); qui si vede subito se il file e valido. Come la shell: niente conferme.
#include "vos_web_int.h"

#if VOS_WITH_ELF
#include "../core/vos_layout.h"
#include "../core/vos_ext.h"
#include "../packages/vos_elf.h"
#include "../packages/vos_elf_file.h"
#include "../packages/vos_progext.h"
#include "../packages/vos_elf_quar.h"
#include "../packages/vos_elf_run.h"
#include "../packages/vos_vde.h"
#include "../packages/vos_func.h"
#include "../packages/vos_prog.h"
#include "../packages/vos_prog_data.h"

namespace {

struct Ctx2 { HeapBuf* j; bool primo; };

void voce(const char* nome, size_t byte, void* ctx) {
  Ctx2* c = (Ctx2*)ctx;
  char path[FS_PATH_MAX + 1];
  progPercorso(path, sizeof path, layoutPath(LAY_APPS), nome, PEXT_APP_E);
  size_t b = byte;
  uint32_t ram = 0;
  int e = vos_elf_pkg_controlla(path, &b, &ram);
  if (!c->primo) c->j->add(',');
  c->primo = false;
  c->j->add("{\"n\":\""); c->j->addJson(nome);
  c->j->addf("\",\"b\":%u,\"ram\":%u,\"ok\":%s,\"q\":%s", (unsigned)byte, (unsigned)ram, e == VOS_ELF_OK ? "true" : "false", vos_elf_quar_dentro(nome) ? "true" : "false");
  if (e != VOS_ELF_OK) { c->j->add(",\"e\":\""); c->j->addJson(vos_elf_err_testo(e)); c->j->add('"'); }
  char use[VOS_ELF_USE_MAX];                                   // i permessi che il modulo chiede (dai simboli che importa) e, se ce n'e, quello che l'utente ha tolto
  bool dati = false;                                           // il modulo importa vos_data_*: salva dati (la pagina lascia scegliere flash o microSD)
  vos_elf_pkg_gruppi(path, use, sizeof use, &dati);
  c->j->add(",\"u\":\""); c->j->addJson(use); c->j->add('"');
  if (dati) c->j->add(",\"d\":true");
  if (const char* off = funcMissing(use)) { c->j->add(",\"m\":\""); c->j->addJson(off); c->j->add('"'); }
  c->j->add('}');
}

bool elencoJson(HeapBuf& j) {
  Ctx2 c = { &j, true };
  j.add("{\"mods\":[");
  vos_elf_pkg_elenco(voce, &c);
  j.add("],\"ps\":[");
  vos_elf_ps_t v[VOS_ELF_RUN_MAX + VOS_ELF_ULTIMI];
  int n = vos_elf_ps(v, VOS_ELF_RUN_MAX + VOS_ELF_ULTIMI);
  for (int i = 0; i < n; i++) {
    if (i) j.add(',');
    j.addf("{\"id\":%d,\"n\":\"", v[i].id);
    j.addJson(v[i].nome);
    j.addf("\",\"a\":%s,\"s\":%u,\"rc\":%d,\"u\":%d}", v[i].attivo ? "true" : "false", (unsigned)v[i].secondi, v[i].ritorno, v[i].uscita);
  }
  j.add("]}");
  return !(j.oom() || j.over());
}

// Avvia un modulo installato: stesso cammino della shell (run) e dell'avvio automatico; i gruppi sono quelli che il modulo chiede.
esp_err_t avvia(Req* r, Res* s, Ctx& c) {
  char path[FS_PATH_MAX + 1], nome[VOS_ELF_NOME_MAX + 1];
  int e = vos_elf_risolvi(Pc(r, "name"), path, sizeof path, nome);
  if (e != VOS_ELF_OK) return ko(s, vos_elf_err_testo(e));
  ErrBuf err;
  return progStart(PK_APP_E, nome, c.role, "pagina", err) ? ok(s) : ko(s, err);
}

// Driver in ELF (.vde): l'upload e il normale caricamento file verso /devs; qui si dano e si tolgono i pin.
esp_err_t driverAzione(Req* r, Res* s) {
  if (Pis(r, "act", "install")) {
    ErrBuf e;
    return vdeInstalla(Pc(r, "name"), Pc(r, "pins"), e) ? ok(s) : ko(s, e.c_str());
  }
  if (Pis(r, "act", "pins")) {                                // altri pin a un driver installato (se non vanno restano i vecchi)
    ErrBuf e;
    return vdeCambiaPin(Pc(r, "name"), Pc(r, "pins"), e) ? ok(s) : ko(s, e.c_str());
  }
  if (Pis(r, "act", "del")) {
    ErrBuf e;
    return vdeRimuovi(Pc(r, "name"), true, e) ? ok(s) : ko(s, e.c_str());
  }
  return ko(s, tr("Azione sconosciuta"));
}

}  // namespace

void routesElf(PsychicHttpServer* S, bool sec) {
  add(S, sec, "/api/elf", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf j(4096);
    if (!elencoJson(j)) return ko(s, tr("Memoria insufficiente"));
    return sendJson(s, j);
  });
  add(S, sec, "/api/vde", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf j(VDE_LIST_JSON_MAX);
    if (!vdeElencoJson(j)) return ko(s, tr("Memoria insufficiente"));
    return sendJson(s, j);
  });
  add(S, sec, "/api/vde", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (!vos_elf_abilitato()) return ko(s, vos_elf_err_testo(VOS_ELF_E_SPENTO));
    return driverAzione(r, s);
  });
  add(S, sec, "/api/elf", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (!vos_elf_abilitato()) return ko(s, vos_elf_err_testo(VOS_ELF_E_SPENTO));
    if (Pis(r, "act", "run")) return avvia(r, s, c);
    if (Pis(r, "act", "kill")) {
      int e = vos_elf_ferma(Pc(r, "name"));
      return e == VOS_ELF_OK ? ok(s) : ko(s, vos_elf_err_testo(e));
    }
    if (Pis(r, "act", "unq")) return vos_elf_quar_togli(Pc(r, "name")) ? ok(s) : ko(s, tr("Non e in quarantena"));
    return ko(s, tr("Azione sconosciuta"));
  });
}
#endif
