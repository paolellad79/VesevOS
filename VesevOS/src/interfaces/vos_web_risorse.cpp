// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_risorse.cpp
// Caricamento dei file di /usr/share/vesevos: POST /api/risorse/carica, multipart, solo Amministratore, fuori dalla tabella.
// Due strade, scelte dal nome del file inviato (solo per scegliere: il nome vero lo da l'elenco):
//  - un file singolo noto (comuni, licenze, manuale, radio) o SHA256SUMS stesso: si accetta solo se lo SHA-256 e quello della riga dell'elenco
//    della scheda (per SHA256SUMS: se l'id e quello del firmware); tetto di dimensione per file; scrittura atomica (core/vos_risorse.cpp);
//  - la pagina html.vrs (e ogni altro .vrs): intestazione, id, tetto (256 KB), lunghezza e SHA-256 si controllano PRIMA di sostituire il file.
// Se qualcosa non va, il file installato resta intatto.
#include "vos_web_int.h"
#include "../core/vos_risorse.h"
#include "../core/vos_ramwd.h"

namespace webx {

namespace {

RisCarico*     g_ris = nullptr;      // caricamento di un .vrs in corso (uno solo alla volta, come g_upFile)
RisFileCarico* g_fcar = nullptr;     // caricamento di un file singolo in corso (mai insieme a g_ris)
RisEsito       g_risEsito = RIS_OK;
bool           g_risFinito = false;  // l'ultimo pezzo e arrivato e il file e stato scambiato
bool           g_elencoKo = false;   // il rifiuto e perche l'elenco SHA256SUMS della scheda non e valido
char           g_risNome[RIS_FILE_NOME_MAX + 1] = "";   // nome del file singolo caricato (dall'elenco)

// Testo per chi carica e codice breve dell'errore.
const char* testoEsito(RisEsito e, const char** code) {
  *code = "error";
  switch (e) {
    case RIS_MANCA:     return g_elencoKo ? tr("L'elenco SHA256SUMS della scheda non e valido: carica prima SHA256SUMS")
                                          : tr("Il file non e nell'elenco SHA256SUMS della scheda");
    case RIS_SPAZIO:    *code = "state"; return tr("Spazio insufficiente per sostituire il file");
    case RIS_GRANDE:    *code = "toobig"; return tr("File troppo grande");
    case RIS_SHA:       return tr("Il file e alterato: il controllo SHA-256 non torna");
    case RIS_LETTURA:
    case RIS_SCRITTURA: return tr("Scrittura non riuscita");
    default:            return tr("File non valido o incompleto");
  }
}

void risAnnullaCorrente() {
  if (g_ris) risCaricoAnnulla(g_ris);
  g_ris = nullptr;
  if (g_fcar) risFileCaricoAnnulla(g_fcar);
  g_fcar = nullptr;
}

// Solo il nome, senza cartelle.
const char* soloNome(const char* n) {
  const char* sl = strrchr(n, '/');
  return sl ? sl + 1 : n;
}

// Primo pezzo: sceglie la strada. Un file singolo noto (o SHA256SUMS) passa dall'elenco; il resto dal controllo .vrs come prima.
void iniziaCarico(const char* nomeInviato) {
  risAnnullaCorrente();
  g_risEsito = RIS_OK;
  g_risFinito = false;
  g_elencoKo = false;
  const char* nome = soloNome(nomeInviato);
  if (risFileCaricabile(nome)) {
    g_fcar = risFileCaricoInizia(nome, g_risEsito);
    g_elencoKo = !g_fcar && risElencoStato() != RIS_OK;
    return;
  }
  g_ris = risCaricoInizia();
  if (!g_ris) g_risEsito = RIS_SCRITTURA;                            // manca la memoria (circa 700 byte)
}

// Un pezzo di dati; all'ultimo pezzo controlla, scambia e ricorda l'esito in g_risEsito / g_risFinito.
void daiCarico(const uint8_t* data, size_t len, bool final) {
  if (g_fcar) {
    g_risEsito = risFileCaricoDai(g_fcar, data, len);
    if (g_risEsito != RIS_OK) { risAnnullaCorrente(); return; }
    if (!final) return;
    snprintf(g_risNome, sizeof g_risNome, "%s", risFileCaricoNome(g_fcar));
    g_risEsito = risFileCaricoFine(g_fcar);                          // controlla, scambia, ricontrolla e libera sempre
    g_fcar = nullptr;
    g_risFinito = g_risEsito == RIS_OK;
    return;
  }
  g_risEsito = risCaricoDai(g_ris, data, len);
  if (g_risEsito != RIS_OK) { risAnnullaCorrente(); return; }
  if (!final) return;
  g_risEsito = risCaricoFine(g_ris);                                 // controlla tutto, scambia e libera sempre
  g_ris = nullptr;
  g_risFinito = g_risEsito == RIS_OK;
  g_risNome[0] = 0;
  if (g_risFinito) {
    paginaEsitoAzzera();                                             // il file buono e cambiato: "/" lo ricontrolla per intero
    risFileAzzeraUno(RF_HTML);                                       // e lo stato del file html.vrs si rifa' (controllo dopo l'avvio)
  }
}

bool adminOk(Req* r, bool sec) {
  if (!fwAllow(ipOf(r), false)) return false;
  if (csrfBlocked(r)) return false;                                  // caricamento partito da un altro sito
  int u = userOf(r);
  if (u < 0 || cfg.users[u].role < ROLE_ADMIN) return false;
  if (!sec && g_tlsUp && cfg.https) return false;
  return true;
}

}  // namespace

void routeRisorse(PsychicHttpServer* S, bool sec) {
  PsychicUploadHandler* up = new PsychicUploadHandler();
  up->onUpload([sec](Req* r, const auto& filename, uint64_t index, uint8_t* data, size_t len, bool final) -> esp_err_t {
    if (!adminOk(r, sec)) return ESP_FAIL;                           // il controllo d'accesso vale anche durante l'invio dei dati
    ramBusyTouch(60);                                                // caricamento in corso: niente riavvio per RAM
    if (index == 0) iniziaCarico(textOf(filename));
    if (!g_ris && !g_fcar) return ESP_OK;                            // errore gia deciso: si leggono e si buttano i dati che restano
    daiCarico(data, len, final);
    return ESP_OK;
  });
  up->onRequest([](Req* r, Res* s) -> esp_err_t {
    int u = userOf(r);
    if (u < 0 || cfg.users[u].role < ROLE_ADMIN) return sendJson(s, "{\"ok\":false,\"err\":\"non autorizzato\"}", 401);
    if (g_risFinito) {
      g_risFinito = false;
      vlog("RISORSE: file %s caricato da %s", g_risNome[0] ? g_risNome : "(.vrs)", cfg.users[u].name.c_str());
      if (g_risNome[0]) auditEvent(AUD_INFO, "", trf("file di risorse %s sostituito da %s", g_risNome, cfg.users[u].name.c_str()).c_str());
      else auditEvent(AUD_INFO, "", trf("risorse della pagina (.vrs) sostituite da %s", cfg.users[u].name.c_str()).c_str());
      return ok(s);
    }
    risAnnullaCorrente();                                            // caricamento interrotto o rifiutato: niente resta a meta
    const char* code;
    const char* testo = testoEsito(g_risEsito == RIS_OK ? RIS_TRONCATA : g_risEsito, &code);
    g_risEsito = RIS_OK;
    return ko(s, testo, code);
  });
  S->on("/api/risorse/carica", HTTP_POST, up);
}

}  // namespace webx
