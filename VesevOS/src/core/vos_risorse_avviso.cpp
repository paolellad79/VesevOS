// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_risorse_avviso.cpp
// Vedi vos_risorse_avviso.h. Scelta: il controllo gira nel ciclo principale (risorseTick, chiamato a ogni giro), un pezzo da 1 KB per giro, e
// il file si RIAPRE a ogni pezzo (nessun file aperto tra un giro e l'altro: su LittleFS un file aperto tiene RAM, e la C3 ne ha poca). Niente
// task nuovo (niente stack in piu): il ciclo ha gia il suo watchdog e fa solo lavoro breve. L'esito va in vos_risorse_file (risFileImposta).
#include "vos_risorse_avviso.h"
#include <stdio.h>
#include <string.h>
#include "vos_audit.h"
#include "vos_i18n.h"
#include "vos_log.h"
#include "vos_sys.h"
#include "vos_risorse_sha.h"
#include "vos_region.h"

namespace {

const uint32_t ATTESA_SEC = 10;             // il controllo parte dopo l'avvio: prima Wi-Fi e server
const size_t   PEZZO = 1024;                // byte letti per giro del ciclo
const uint8_t  BIT_ELENCO = 1u << 7;        // nel registro degli avvisi dati: l'elenco SHA256SUMS (i file sono 7: bit 0-6)

uint8_t g_dati = 0;                         // avvisi gia dati (un bit per file): si ridanno solo se il file torna buono e si rompe di nuovo

bool problema(RisStato s) { return s == RS_DIVERSO || s == RS_MANCANTE; }
const char* motivo(RisStato s) { return s == RS_MANCANTE ? tr("mancante") : tr("diverso dall'elenco"); }

void avvisa(const char* nome, RisStato s) {
  vlog("RISORSE: %s %s: scaricalo di nuovo (Sicurezza > Aggiornamenti)", nome, s == RS_MANCANTE ? "mancante" : "diverso dall'elenco");
  auditEvent(AUD_YELLOW, "", trf("file di risorse %s: %s. Scaricalo di nuovo: Sicurezza > Aggiornamenti", nome, motivo(s)).c_str());
}

// Avvisi nuovi: guarda gli stati (pochi byte) e avvisa una volta per ogni problema nuovo.
void segnala() {
  for (int i = 0; i < RF_N; i++) {
    RisStato s = risFileStato((RisFile)i);
    uint8_t bit = (uint8_t)(1u << i);
    if (problema(s) && !(g_dati & bit)) {
      g_dati |= bit;
      avvisa(risFileNome((RisFile)i), s);
    } else if (!problema(s)) {
      g_dati &= (uint8_t)~bit;
    }
  }
  bool elencoKo = risElencoNoto() && risElencoStato() != RIS_OK;
  if (elencoKo && !(g_dati & BIT_ELENCO)) {
    g_dati |= BIT_ELENCO;
    avvisa("SHA256SUMS", risElencoStato() == RIS_MANCA ? RS_MANCANTE : RS_DIVERSO);
  } else if (!elencoKo) {
    g_dati &= (uint8_t)~BIT_ELENCO;
  }
}

// Il prossimo file ancora da controllare (RF_N = nessuno).
RisFile prossimo() {
  for (int i = 0; i < RF_N; i++)
    if (risFileStato((RisFile)i) == RS_NON_CONTROLLATO) return (RisFile)i;
  return RF_N;
}

// Una volta per avvio: radio.txt buono, si guardano i suoi valori massimi (avviso, nessun blocco).
void controlloRadio() {
  static bool fatto = false;
  if (fatto || risFileStato(RF_RADIO) != RS_OK) return;
  fatto = true;
  regionTabellaAvviso();
}

void riassunto(uint32_t gen) {
  static uint32_t detto = 0xFFFFFFFFu;
  if (detto == gen) return;
  detto = gen;
  int ok = 0, no = 0;
  for (int i = 0; i < RF_N; i++) {
    if (risFileStato((RisFile)i) == RS_OK) ok++;
    else no++;
  }
  vlog("RISORSE: controllo dei file di /usr/share/vesevos finito (%d buoni, %d con problemi)", ok, no);
}

}  // namespace

void risorseTick() {
  static RisFile cur = RF_N;                // file in controllo (RF_N = nessuno)
  static size_t pos = 0;
  static size_t lunghezza = 0;
  static uint32_t gen = 0;
  static mbedtls_sha256_context ctx;
  static RisConfronto cf(risShaMbed(&ctx));
  if (sysUptimeSec() < ATTESA_SEC) return;
  segnala();
  controlloRadio();
  if (cur == RF_N) {
    if (!risElencoNoto()) risElencoAvvio();
    if (risElencoStato() != RIS_OK) return;                // elenco non valido o mancante: i servizi usano il ripiego, niente da controllare
    RisFile rf = prossimo();
    if (rf == RF_N) {
      riassunto(risFileGen());
      return;
    }
    uint8_t atteso[32];
    RisEsito e = risFileAtteso(rf, atteso);
    if (e != RIS_OK) {
      if (risElencoStato() == RIS_OK) risFileImposta(rf, RS_DIVERSO);   // il file non e nell'elenco
      return;
    }
    VosFile f;
    e = risFileApriGrezzo(rf, f, lunghezza);
    if (e != RIS_OK) {
      risFileImposta(rf, risStatoDaEsito(e));
      return;
    }
    cf.avvia(atteso);
    cur = rf;
    pos = 0;
    gen = risFileGen();
    return;
  }
  size_t len = 0;
  VosFile f;
  RisEsito e = gen == risFileGen() ? risFileApriGrezzo(cur, f, len) : RIS_LETTURA;
  if (gen != risFileGen() || (e == RIS_OK && len != lunghezza)) {      // file cambiato nel frattempo: si riparte da capo
    cf.annulla();
    cur = RF_N;
    return;
  }
  if (e != RIS_OK) {
    cf.annulla();
    risFileImposta(cur, risStatoDaEsito(e));
    cur = RF_N;
    return;
  }
  uint8_t buf[PEZZO];
  size_t voglio = lunghezza - pos < PEZZO ? lunghezza - pos : PEZZO;
  size_t letti = f.seek(pos) ? f.read(buf, voglio) : 0;
  if (letti == 0) {                                        // finito troppo presto o errore di lettura
    cf.annulla();
    risFileImposta(cur, RS_DIVERSO);
    cur = RF_N;
    return;
  }
  cf.dai(buf, letti);
  pos += letti;
  if (pos < lunghezza) return;
  bool uguale = cf.fine();
  if (gen == risFileGen()) risFileImposta(cur, uguale ? RS_OK : RS_DIVERSO);
  cur = RF_N;
}

bool risorseProblema(char* nome, size_t cap, RisStato& stato, int& quanti) {
  quanti = 0;
  bool primo = false;
  if (risElencoNoto() && risElencoStato() != RIS_OK) {
    snprintf(nome, cap, "SHA256SUMS");
    stato = risElencoStato() == RIS_MANCA ? RS_MANCANTE : RS_DIVERSO;
    quanti = 1;
    primo = true;
  }
  for (int i = 0; i < RF_N; i++) {
    RisStato s = risFileStato((RisFile)i);
    if (!problema(s)) continue;
    quanti++;
    if (primo) continue;
    snprintf(nome, cap, "%s", risFileNome((RisFile)i));
    stato = s;
    primo = true;
  }
  return quanti > 0;
}

bool risorseJson(HeapBuf& j) {
  char elenco[RIS_ELENCO_MAX];
  size_t n = 0;
  RisEsito e = risElencoLeggi(elenco, n);                  // rilegge l'elenco: serve il testo per cercare gli hash
  bool buono = e == RIS_OK;
  j.addf("{\"el\":%d,\"f\":[", buono ? 0 : (e == RIS_MANCA ? 1 : 2));
  for (int i = 0; i < RF_N; i++) {
    uint8_t h[32];
    char hex[65] = "";
    if (buono && risElencoCerca(elenco, n, risFileNome((RisFile)i), h)) {
      for (int b = 0; b < 32; b++) snprintf(hex + b * 2, 3, "%02x", h[b]);
    }
    j.addf("%s{\"n\":\"%s\",\"s\":%d,\"h\":\"%s\",\"t\":%lu}", i ? "," : "", risFileNome((RisFile)i), (int)risFileStato((RisFile)i), hex,
           (unsigned long)risFileTetto((RisFile)i));
  }
  return j.add("]}");
}

bool risorseNotaJson(HeapBuf& j) {
  char nome[RIS_FILE_NOME_MAX + 1] = "";
  RisStato s = RS_OK;
  int quanti = 0;
  risorseProblema(nome, sizeof nome, s, quanti);
  return j.addf("{\"k\":%d,\"n\":\"%s\",\"s\":%d}", quanti, nome, (int)s);
}
