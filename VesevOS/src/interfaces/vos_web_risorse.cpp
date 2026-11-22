// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_risorse.cpp
// Caricamento dei file di /usr/share/vesevos: POST /api/risorse/carica, multipart, solo Amministratore, fuori dalla tabella.
// Due strade, scelte dal nome del file inviato (solo per scegliere: il nome vero lo da l'elenco):
//  - un file singolo di un ELENCO CHIUSO di nomi (comuni, licenze, manuale, radio) o SHA256SUMS stesso: se il file porta la sua impronta interna
//    (VOSR formato 2, oppure testo con la prima riga "# sha256=") la scheda la controlla e aggiorna da sola la sua riga in SHA256SUMS; se no
//    si accetta solo se lo SHA-256 e quello della riga dell'elenco della scheda; tetto di dimensione per file; scrittura atomica (core/vos_risorse.cpp);
//  - la pagina html.vrs (e ogni altro .vrs): intestazione, id, tetto (256 KB), lunghezza e SHA-256 si controllano PRIMA di sostituire il file;
//    anche la sua riga in SHA256SUMS si aggiorna da sola.
// Se qualcosa non va, il file installato resta intatto.
// Terza strada, per le schede con poca RAM contigua (C3, senza PSRAM): POST /api/risorse/pezzo?n=<nome>&o=<byte gia inviati>&e=<1 se ultimo>, corpo
// grezzo di al massimo 8 KB (la pagina di emergenza ne manda 4 KB). Un record TLS piccolo non chiede il blocco da 16 KB che li' non si trova.
// Stesse regole e stessi controlli delle altre due strade (stesso RisCarico / RisFileCarico); il pezzo con offset 0 ricomincia, gli altri devono
// seguire esattamente quelli gia accettati: se no la risposta dice a che punto e la scheda (n) e chi carica riparte da li.
#include "vos_web_int.h"
#include "vos_web_pezzi.h"
#include "../core/vos_risorse.h"
#include "../core/vos_ramwd.h"

namespace webx {

namespace {

RisCarico*     g_ris = nullptr;      // caricamento di un .vrs in corso (uno solo alla volta, come g_upFile)
RisFileCarico* g_fcar = nullptr;     // caricamento di un file singolo in corso (mai insieme a g_ris)
RisEsito       g_risEsito = RIS_OK;
bool           g_risFinito = false;  // l'ultimo pezzo e arrivato e il file e stato scambiato
char           g_risNome[RIS_FILE_NOME_MAX + 1] = "";   // nome del file singolo caricato (dall'elenco)
uint32_t       g_pPos = 0;            // byte accettati finora del file in caricamento a pezzi
bool           g_pUltimo = false;     // il pezzo in arrivo e l'ultimo
bool           g_pFuori = false;      // l'offset del pezzo non e quello atteso: non si usa, si dice a che punto siamo
bool           g_pTroppo = false;     // il pezzo e piu grande di PEZZO_MAX

// Testo per chi carica e codice breve dell'errore.
const char* testoEsito(RisEsito e, const char** code) {
  *code = "error";
  switch (e) {
    case RIS_MANCA:     return risElencoStato() != RIS_OK ? tr("L'elenco SHA256SUMS della scheda non e valido: carica prima SHA256SUMS")
                                                      : tr("Il file non e nell'elenco SHA256SUMS della scheda");
    case RIS_SPAZIO:    *code = "state"; return tr("Spazio insufficiente per sostituire il file");
    case RIS_GRANDE:    *code = "toobig"; return tr("File troppo grande");
    case RIS_DIPENDE:   *code = "state"; return tr("Questo file richiede un firmware piu nuovo di quello in uso: aggiorna prima il firmware");
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
  const char* nome = soloNome(nomeInviato);
  if (risFileCaricabile(nome)) {
    g_fcar = risFileCaricoInizia(nome, g_risEsito);
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

// Registro e audit del file appena sostituito (uguale per tutte le strade).
void registraFine(int u) {
  vlog("RISORSE: file %s caricato da %s", g_risNome[0] ? g_risNome : "(.vrs)", cfg.users[u].name.c_str());
  if (g_risNome[0]) auditEvent(AUD_INFO, "", trf("file di risorse %s sostituito da %s", g_risNome, cfg.users[u].name.c_str()).c_str());
  else auditEvent(AUD_INFO, "", trf("risorse della pagina (.vrs) sostituite da %s", cfg.users[u].name.c_str()).c_str());
}

// Inizio di un pezzo: legge nome, offset e "ultimo" dalla riga dell'indirizzo e decide se il pezzo si usa.
void pezzoInizio(Req* r) {
  g_pFuori = g_pTroppo = false;
  char n[RIS_FILE_NOME_MAX + 1];
  qparamTo(r, "n", n, sizeof n);
  PezzoQ q = pezzoQuery(r);
  g_pUltimo = q.ultimo;
  if (q.troppo) { g_pTroppo = true; return; }
  uint32_t off = q.off;
  if (off == 0) { iniziaCarico(n); g_pPos = 0; return; }              // si ricomincia da capo (il caricamento vecchio si butta)
  if ((!g_ris && !g_fcar) || off != g_pPos) g_pFuori = true;          // non e il pezzo che si aspetta la scheda
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
      registraFine(u);
      return ok(s);
    }
    risAnnullaCorrente();                                            // caricamento interrotto o rifiutato: niente resta a meta
    const char* code;
    const char* testo = testoEsito(g_risEsito == RIS_OK ? RIS_TRONCATA : g_risEsito, &code);
    g_risEsito = RIS_OK;
    return ko(s, testo, code);
  });
  S->on("/api/risorse/carica", HTTP_POST, up);

  // Caricamento a pezzi (vedi l'intestazione del file).
  PsychicUploadHandler* pz = new PsychicUploadHandler();
  pz->onUpload([sec](Req* r, const auto& filename, uint64_t index, uint8_t* data, size_t len, bool final) -> esp_err_t {
    (void)filename;
    if (!adminOk(r, sec)) return ESP_FAIL;
    ramBusyTouch(60);
    if (index == 0) pezzoInizio(r);
    if (g_pFuori || g_pTroppo || (!g_ris && !g_fcar)) return ESP_OK;  // pezzo da scartare o errore gia deciso: si leggono e si buttano i dati
    daiCarico(data, len, final && g_pUltimo);
    if (g_ris || g_fcar || g_risFinito) g_pPos += (uint32_t)len;      // accettato
    return ESP_OK;
  });
  pz->onRequest([](Req* r, Res* s) -> esp_err_t {
    int u = userOf(r);
    if (u < 0 || cfg.users[u].role < ROLE_ADMIN) return sendJson(s, "{\"ok\":false,\"err\":\"non autorizzato\"}", 401);
    if (g_risFinito) {                                                // ultimo pezzo: il file e stato controllato e scambiato
      g_risFinito = false;
      g_pPos = 0;
      registraFine(u);
      return ok(s);
    }
    if (g_pFuori) {                                                   // pezzo fuori posto: il caricamento in corso resta, si dice a che punto e
      return pezzoFuoriPosto(s, (g_ris || g_fcar) ? g_pPos : 0);
    }
    if (g_pTroppo) return ko(s, tr("Pezzo del file troppo grande"), "toobig");
    if ((g_ris || g_fcar) && g_risEsito == RIS_OK) {                  // pezzo accettato, il file non e finito
      return pezzoAccettato(s, g_pPos);
    }
    risAnnullaCorrente();                                             // errore: niente resta a meta
    g_pPos = 0;
    const char* code;
    const char* testo = testoEsito(g_risEsito == RIS_OK ? RIS_TRONCATA : g_risEsito, &code);
    g_risEsito = RIS_OK;
    return ko(s, testo, code);
  });
  S->on("/api/risorse/pezzo", HTTP_POST, pz);
}

}  // namespace webx
