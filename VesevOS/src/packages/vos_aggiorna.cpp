// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_aggiorna.cpp
#include "vos_aggiorna.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../core/vos_common.h"
#include "../core/vos_config.h"
#include "../core/vos_audit.h"
#include "../core/vos_i18n.h"
#include "../core/vos_lock.h"
#include "../core/vos_log.h"
#include "../core/vos_pacchetto.h"
#include "../core/vos_recupd.h"
#include "../core/vos_sys.h"
#include "../core/vos_time.h"
#include "../core/vos_updorder.h"
#include "../core/vos_wd.h"
#include "../devices/vos_sd.h"
#include "../drivers/vos_drv_heap.h"
#include "../drivers/vos_drv_https.h"
#include "../drivers/vos_drv_nvs.h"
#include "../drivers/vos_drv_ota.h"
#include "../drivers/vos_drv_task.h"
#include "../drivers/vos_drv_time.h"
#include "../net/vos_net.h"
#include "vos_aggiorna_dati.h"
#include "vos_aggiorna_int.h"
#include "vos_aggiorna_sd.h"

namespace {

const char* kUrlUltima = "https://raw.githubusercontent.com/paolellad79/VesevOS/main/aggiorna/latest.json";
const size_t kTettoUltima = 4096;
const uint32_t kTempoLettura = 30000;
const uint32_t kStack = 12288;                  // il certificato del server si controlla su questo stack; la FAT con i nomi lunghi usa lo stesso
const uint32_t kRamMinima = 50000;              // circa 22 KB per il collegamento, piu il margine per il resto della scheda
const uint32_t kBloccoMinimo = 18000;           // un record TLS intero deve entrare
const uint32_t kOgniMs = 20000;                 // ogni quanto il ciclo principale guarda l'orario
const uint32_t kProvaSec = 180;                 // un firmware appena installato che regge 3 minuti si conferma
const uint32_t kPrimaDiNuovoSec = 6 * 3600;     // col cambio dell'ora la stessa ora torna due volte: un controllo solo
const char* kNvs = "vosagg";
const char* kNvsVersione = "tver";              // ultima versione tentata da sola
const char* kNvsTentativi = "tnum";             // quante volte e andata nel recovery
const char* kNvsDetto = "tdet";                 // la fermata e gia stata segnalata

enum Esito : uint8_t { ESITO_NESSUNO, ESITO_OK, ESITO_RETE, ESITO_ORA, ESITO_RAM, ESITO_COLLEGAMENTO, ESITO_DATI };

VosMutex s_blocco;
volatile bool s_corso = false;
Esito s_esito = ESITO_NESSUNO;
uint32_t s_ultimo = 0;                          // epoch dell'ultimo controllo finito
bool s_trovato = false;                         // s_disp e valido
AggDisponibile s_disp;
bool s_notaVista = false;
bool s_daSegnalare = false;                     // microSD guasta o tentativi finiti: si dice una volta nelle notifiche
char s_segnalata[24] = "";                      // versione gia scritta nel registro: una riga sola per versione
char s_motivo[64] = "";                         // installazione automatica non partita

const char* testoEsito(Esito e) {
  switch (e) {
    case ESITO_OK: return tr("Controllo riuscito");
    case ESITO_RETE: return tr("La scheda non e collegata alla Wi-Fi di casa");
    case ESITO_ORA: return tr("Ora non disponibile: controllo saltato");
    case ESITO_RAM: return tr("Memoria insufficiente: controllo saltato");
    case ESITO_COLLEGAMENTO: return tr("Sito del progetto non raggiungibile");
    case ESITO_DATI: return tr("Elenco degli aggiornamenti non valido");
    default: return "";
  }
}

void campo(HeapBuf& j, const char* chiave, const char* valore) {
  j.addf(",\"%s\":\"", chiave);
  j.addJson(valore);
  j.add('"');
}

bool ramSufficiente() { return drvHeapFree() >= kRamMinima && drvHeapMaxBlock() >= kBloccoMinimo; }

bool firmwarePiuNuovo(const AggDisponibile& d) { return pacConfrontaFirmware(d.ver, VOS_VERSION) == 1; }

// Ufficiale per davvero: canale dichiarato E indirizzo tra quelli ufficiali.
bool ufficiale(const AggDisponibile& d) { return !strcmp(d.canale, "official") && updUrlUfficiale(d.url); }

// Scarica e legge aggiorna/latest.json.
Esito leggiUltima(AggDisponibile& d) {
  char* buf = (char*)drvMallocBig(kTettoUltima + 1);
  if (!buf) return ESITO_RAM;
  size_t n = 0;
  int stato = 0;
  DrvHttpsEsito e = drvHttpsScarica(kUrlUltima, buf, kTettoUltima + 1, n, kTempoLettura, stato);
  Esito esito = ESITO_OK;
  if (e == HTTPS_E_MEMORIA) esito = ESITO_RAM;
  else if (e == HTTPS_E_TROPPO) esito = ESITO_DATI;
  else if (e != HTTPS_OK) esito = ESITO_COLLEGAMENTO;
  else if (!aggLeggiUltima(buf, n, d)) esito = ESITO_DATI;
  if (esito != ESITO_OK) vlog("AGGIORNAMENTI: elenco non letto (rete %d, HTTP %d)", (int)e, stato);
  free(buf);
  return esito;
}

void segnalaDisponibile(const AggDisponibile& d) {
  if (!firmwarePiuNuovo(d) || !strcmp(s_segnalata, d.ver)) return;
  snprintf(s_segnalata, sizeof s_segnalata, "%s", d.ver);
  auditEvent(AUD_INFO, "", trf("aggiornamento disponibile: firmware %s", d.ver).c_str());
}

void salvaUltimo(uint32_t quando) {
  DrvNvs n;
  if (!n.begin(kNvs, false)) return;
  n.putUInt("ult", quando);
  n.end();
}

struct Tentativi {
  char ver[24];
  uint32_t fatti;
  bool detto;
};

void leggiTentativi(Tentativi& t) {
  memset(&t, 0, sizeof t);
  DrvNvs n;
  if (!n.begin(kNvs, true)) return;
  n.getString(kNvsVersione, t.ver, sizeof t.ver);
  t.fatti = n.getUInt(kNvsTentativi, 0);
  t.detto = n.getBool(kNvsDetto, false);
  n.end();
}

void salvaTentativi(const Tentativi& t) {
  DrvNvs n;
  if (!n.begin(kNvs, false)) return;
  n.putString(kNvsVersione, t.ver);
  n.putUInt(kNvsTentativi, t.fatti);
  n.putBool(kNvsDetto, t.detto);
  n.end();
}

// Si conta prima del riavvio: l'esito arriva da un altro avvio, e se non riesce la versione non cambia.
void contaTentativo(const char* versione) {
  Tentativi t;
  leggiTentativi(t);
  if (strcmp(t.ver, versione)) {
    memset(&t, 0, sizeof t);
    snprintf(t.ver, sizeof t.ver, "%s", versione);
  }
  t.fatti++;
  salvaTentativi(t);
}

void azzeraTentativiSeInstallata() {
  Tentativi t;
  leggiTentativi(t);
  if (!aggTentativiDaAzzerare(t.ver, VOS_VERSION)) return;
  memset(&t, 0, sizeof t);
  salvaTentativi(t);
}

// Il recovery ha rimesso la copia di riserva: quella versione non si riprova, la prossima si.
void scartaVersione(const char* versione) {
  if (!versione[0]) return;
  Tentativi t = {};
  snprintf(t.ver, sizeof t.ver, "%s", versione);
  t.fatti = AGG_TENTATIVI_MAX;
  t.detto = true;                               // la notifica c'e gia: e l'esito del recovery
  salvaTentativi(t);
}

// Il firmware nuovo ha retto: la riserva resta sulla microSD ma non serve piu all'avvio.
void chiudiProva() {
  if (!drvOtaInProva()) return;
  drvOtaConfermaProva();
  updRiservaDisarma();
  aggSdPulisci();
  vlog("AGGIORNAMENTI: firmware nuovo confermato");
}

// false = tentativi finiti per questa versione. La notifica parte una volta sola; il motivo si vede a ogni controllo.
bool tentativiRimasti(const AggDisponibile& d) {
  Tentativi t;
  leggiTentativi(t);
  if (aggTentativoAmmesso(t.ver, t.fatti, d.ver)) return true;
  const char* motivo = tr("Installazione fermata: si aspetta una versione nuova");
  {
    VosLock l(s_blocco);
    snprintf(s_motivo, sizeof s_motivo, "%s", motivo);
    if (!t.detto) { s_daSegnalare = true; s_notaVista = false; }
  }
  if (t.detto) return false;
  t.detto = true;
  salvaTentativi(t);
  auditEvent(AUD_YELLOW, "", trf("installazione automatica del firmware %s fermata dopo 3 tentativi", d.ver).c_str());
  return false;
}

// Dalla 0.5.3 il recovery torna da solo a VesevOS se l'installazione automatica non riesce. Versione illeggibile = troppo vecchio.
bool recoveryTorna() {
  int c = pacConfrontaFirmware(recVersion(), "0.5.3");
  return c == 0 || c == 1;
}

// Il riavvio non deve cadere durante un lavoro che non si puo ripetere. false = motivo in s_motivo, si riprova al controllo successivo.
bool installaDaSolo(const AggDisponibile& d) {
  {
    VosLock l(s_blocco);
    s_motivo[0] = 0;
    s_daSegnalare = false;
  }
  const char* ostacolo = nullptr;
  ErrBuf errUrl;
  if (!ufficiale(d)) ostacolo = tr("Canale o indirizzo non ufficiale: niente installazione");
  else if (!updUrlAllowed(d.url, errUrl)) ostacolo = errUrl.c_str();
  else if (!drvRecoveryPresent()) ostacolo = tr("Recovery non presente");
  else if (!recoveryTorna()) ostacolo = tr("Recovery troppo vecchio: aggiornalo per installare da solo");
  else if (sdFormatting()) ostacolo = tr("microSD occupata (formattazione)");
  else if (drvOtaInProva()) ostacolo = tr("Il firmware appena installato e ancora in prova");
  ErrBuf err;
  AggSdEsito sd = AGG_SD_ASSENTE;
  AggRiserva riserva = {};
  if (!ostacolo) {
    sd = aggSdPrepara(d, err);
    if (sd == AGG_SD_PRONTO && !aggSdRiserva(riserva, err)) sd = AGG_SD_ERRORE;   // una microSD che sbaglia la copia non e affidabile
    if (sd == AGG_SD_ERRORE) ostacolo = err.c_str();
    else if (sd == AGG_SD_PIENA) vlog("AGGIORNAMENTI: microSD con poco spazio: si usa la rete");
  }
  if (!ostacolo) {
    bool ordine = sd == AGG_SD_PRONTO ? aggSdOrdine(d, riserva, err) : updOrderNet(d.url, d.sha, d.ver, d.dim, err, true);
    if (!ordine) ostacolo = err.c_str();
  }
  if (ostacolo) {
    vlog("AGGIORNAMENTI: installazione rimandata: %s", ostacolo);
    if (sd == AGG_SD_ERRORE) auditEvent(AUD_YELLOW, "", trf("installazione automatica rimandata: %s", ostacolo).c_str());
    VosLock l(s_blocco);
    snprintf(s_motivo, sizeof s_motivo, "%s", ostacolo);
    if (sd == AGG_SD_ERRORE) { s_daSegnalare = true; s_notaVista = false; }
    return false;
  }
  contaTentativo(d.ver);
  auditEvent(AUD_INFO, "", trf("installazione automatica del firmware %s: la scheda si riavvia nel recovery", d.ver).c_str());
  sysRebootLater("aggiornamento automatico", 2000);
  return true;
}

void controlla(bool programmato) {
  aggSdPulisci();                               // resto di un'installazione gia fatta o interrotta
  Esito esito = ESITO_OK;
  AggDisponibile* d = new AggDisponibile();
  if (netState() != NET_CLIENT_OK) esito = ESITO_RETE;
  else if (!timeValid()) esito = ESITO_ORA;
  else esito = leggiUltima(*d);
  bool letto = esito == ESITO_OK;
  uint32_t adesso = timeValid() ? (uint32_t)time(NULL) : 0;
  {
    VosLock l(s_blocco);
    s_esito = esito;
    if (adesso) s_ultimo = adesso;
    if (letto) { s_disp = *d; s_trovato = true; }
  }
  if (adesso) salvaUltimo(adesso);
  if (letto) segnalaDisponibile(*d);
  else vlog("AGGIORNAMENTI: %s", testoEsito(esito));
  if (letto && cfg.aggModo == 2 && firmwarePiuNuovo(*d) && tentativiRimasti(*d) && programmato) installaDaSolo(*d);
  delete d;
}

void taskControllo(void* programmato) {
  controlla(programmato != nullptr);
  s_corso = false;
  vTaskDelete(NULL);
}

// Il tasto della pagina e l'orario possono arrivare insieme: il blocco fa partire un controllo solo.
bool avvia(bool programmato) {
  VosLock l(s_blocco);
  if (s_corso) return false;
  if (ramSufficiente()) {
    s_corso = true;
    if (drvTaskCreate(taskControllo, "aggiorna", kStack, programmato ? (void*)1 : nullptr, 1, nullptr, DRV_CORE_ANY)) return true;
    s_corso = false;
  }
  s_esito = ESITO_RAM;
  return false;
}

}  // namespace

void aggiornaInit() {
  s_blocco.init();
  DrvNvs n;
  if (!n.begin(kNvs, true)) return;
  s_ultimo = n.getUInt("ult", 0);
  n.end();
  azzeraTentativiSeInstallata();
  if (updOutcome().res == UPD_E_RIPRISTINO) scartaVersione(updOutcome().ver);
}

void aggiornaTick() {
  static uint32_t guardato = 0;
  static uint32_t ultimoProgrammato = 0;
  static bool oraSegnalata = false;
  if (drvMillis() - guardato < kOgniMs) return;
  guardato = drvMillis();
  if (sysUptimeSec() >= kProvaSec) chiudiProva();             // prima del selettore: va chiusa anche se nel frattempo lo hanno spento
  if (!cfg.aggModo || sysUptimeSec() < 120) return;           // subito dopo un riavvio non si controlla: e cosi che si evita di ripartire in tondo
  if (!timeValid()) {
    if (!oraSegnalata && sysUptimeSec() > 600) { oraSegnalata = true; vlog("AGGIORNAMENTI: %s", testoEsito(ESITO_ORA)); }
    return;
  }
  time_t adesso = time(NULL);
  struct tm locale;
  localtime_r(&adesso, &locale);
  if (!aggOrarioGiusto(locale, cfg.aggFreq, cfg.aggGiorno, cfg.aggOra)) return;
  if ((uint32_t)adesso - ultimoProgrammato < kPrimaDiNuovoSec) return;
  ultimoProgrammato = (uint32_t)adesso;
  avvia(true);
}

bool aggiornaVerifica(ErrBuf& err) {
  if (s_corso) { err = tr("Controllo gia in corso"); return false; }
  if (avvia(false)) return true;
  err = testoEsito(ESITO_RAM);
  return false;
}

bool aggiornaInstalla(char* versione, size_t cap, ErrBuf& err) {
  AggDisponibile* d = new AggDisponibile();
  bool trovato;
  {
    VosLock l(s_blocco);
    trovato = s_trovato;
    if (trovato) *d = s_disp;
  }
  bool ok = false;
  if (!trovato) err = tr("Prima fai la verifica online");
  else if (!firmwarePiuNuovo(*d)) err = tr("Non c'e una versione piu nuova");
  else if (!ufficiale(*d)) err = tr("Canale o indirizzo non ufficiale: niente installazione");
  else if (s_corso) err = tr("Controllo gia in corso");
  else ok = updOrderNet(d->url, d->sha, d->ver, d->dim, err);
  if (ok) snprintf(versione, cap, "%s", d->ver);
  delete d;
  return ok;
}

bool aggiornaStatoJson(HeapBuf& j) {
  VosLock l(s_blocco);
  bool nuovo = s_trovato && firmwarePiuNuovo(s_disp);
  j.add("{\"ok\":true");
  campo(j, "fw", VOS_VERSION);
  campo(j, "rec", recVersion());
  j.addf(",\"recOk\":%s,\"modo\":%d,\"freq\":%d,\"giorno\":%d,\"ora\":%d,\"oraOk\":%s,\"corso\":%s,\"ult\":%lu",
         drvRecoveryPresent() ? "true" : "false", (int)cfg.aggModo, (int)cfg.aggFreq, (int)cfg.aggGiorno, (int)cfg.aggOra,
         timeValid() ? "true" : "false", s_corso ? "true" : "false", (unsigned long)s_ultimo);
  campo(j, "esito", testoEsito(s_esito));
  campo(j, "motivo", s_motivo);
  j.addf(",\"trovato\":%s,\"nuovo\":%s,\"uff\":%s,\"dim\":%lu", s_trovato ? "true" : "false", nuovo ? "true" : "false",
         s_trovato && ufficiale(s_disp) ? "true" : "false", (unsigned long)(s_trovato ? s_disp.dim : 0));
  campo(j, "ver", s_trovato ? s_disp.ver : "");
  campo(j, "data", s_trovato ? s_disp.data : "");
  campo(j, "chi", s_trovato ? s_disp.chi : "");
  campo(j, "verRec", s_trovato ? s_disp.verRec : "");
  return j.add('}');
}

bool aggiornaPacchettiJson(HeapBuf& j) { return aggPacchettiJson(j); }

bool aggiornaNotaJson(HeapBuf& j) {
  const UpdOutcome& o = updOutcome();
  VosLock l(s_blocco);
  char testo[28] = "";
  int tipo = AGG_NOTA_NIENTE;
  if (o.res && !s_notaVista) {
    tipo = o.res == UPD_OK ? AGG_NOTA_INSTALLATO : AGG_NOTA_FALLITO;
    snprintf(testo, sizeof testo, "%s", o.ver);
  } else if (s_daSegnalare && !s_notaVista) {
    tipo = AGG_NOTA_FALLITO;
  } else if (s_trovato && firmwarePiuNuovo(s_disp)) {
    tipo = AGG_NOTA_DISPONIBILE;
    snprintf(testo, sizeof testo, "%s", s_disp.ver);
  }
  j.addf("{\"k\":%d,\"m\":%d", tipo, (int)cfg.aggModo);     // m: la pagina confronta i pacchetti con lo store solo a controllo acceso
  campo(j, "v", testo);
  return j.add('}');
}

void aggiornaNotaVista() { s_notaVista = true; }
