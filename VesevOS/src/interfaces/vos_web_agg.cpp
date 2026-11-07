// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_agg.cpp
// Pagina Aggiornamenti (solo Amministratore). Il controllo gira nel suo task: da qui si avvia e si leggono i risultati.
#include "vos_web_int.h"

namespace {

// Dati del selettore gia controllati. false = err dice cosa non va.
bool leggiScelta(Req* r, uint8_t& modo, uint8_t& freq, uint8_t& giorno, uint16_t& ora, ErrBuf& err) {
  long m = Pint(r, "modo"), f = Pint(r, "freq"), g = Pint(r, "giorno"), o = Pint(r, "ora");
  if (m < 0 || m > 2 || f < 0 || f > 2 || o < 0 || o > 1439) { err = tr("Valori non validi"); return false; }
  if (f == 1 && (g < 1 || g > 7)) { err = tr("Giorno della settimana non valido"); return false; }
  if (f == 2 && (g < 1 || g > 28)) { err = tr("Giorno del mese non valido: da 1 a 28"); return false; }
  modo = (uint8_t)m; freq = (uint8_t)f; giorno = f ? (uint8_t)g : 1; ora = (uint16_t)o;
  return true;
}

}  // namespace

void routesAgg(PsychicHttpServer* S, bool sec) {
  add(S, sec, "/api/aggiorna", HTTP_GET, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf j(1024);
    if (!aggiornaStatoJson(j) || j.oom() || j.over()) return ko(s, tr("Memoria insufficiente"), "off");
    aggiornaNotaVista();
    return sendJson(s, j);
  });

  add(S, sec, "/api/aggiorna/pacchetti", HTTP_GET, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf j(4096);
    if (!aggiornaPacchettiJson(j) || j.oom() || j.over()) return ko(s, tr("Memoria insufficiente"), "off");
    return sendJson(s, j);
  });

  // Passare da Spento a un'altra scelta fa partire collegamenti verso GitHub: serve la conferma dell'avviso nella pagina.
  add(S, sec, "/api/aggiorna/imposta", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    uint8_t modo, freq, giorno; uint16_t ora; ErrBuf e;
    if (!leggiScelta(r, modo, freq, giorno, ora, e)) return ko(s, e);
    if (modo && !cfg.aggModo && !Pis(r, "confirm", "1")) return ko(s, tr("Serve la conferma"), "bad_request");
    cfg.aggModo = modo; cfg.aggFreq = freq; cfg.aggGiorno = giorno; cfg.aggOra = ora;
    cfgSave();
    return ok(s);
  });

  add(S, sec, "/api/aggiorna/verifica", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    ErrBuf e;
    return aggiornaVerifica(e) ? ok(s) : ko(s, e, "state");
  });

  add(S, sec, "/api/aggiorna/installa", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (!Pis(r, "confirm", "1")) return ko(s, tr("Serve la conferma"), "bad_request");
    char versione[24]; ErrBuf e;
    if (!aggiornaInstalla(versione, sizeof versione, e)) return ko(s, e, "state");
    char w[96]; whoTo(c, w, sizeof w);
    vlog("SISTEMA: riavvio nel recovery per installare la versione %s (%s)", versione, w);
    auditEvent(AUD_INFO, "", trf("aggiornamento tramite recovery (versione pubblicata %s) richiesto da %s", versione, w).c_str());
    esp_err_t res = ok(s);
    sysRebootLater("recovery per aggiornamento", 800);
    return res;
  });
}
