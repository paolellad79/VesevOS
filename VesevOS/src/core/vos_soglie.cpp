// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_soglie.cpp
#include "vos_soglie.h"
#include "vos_i18n.h"

// Condizioni valide per la sorgente: livello = alto/basso, analogico = sopra/sotto/fuori/dentro.
static bool condizioneValida(int sorgente, int condizione) {
  if (sorgente == DRV_ULP_LIVELLO) return condizione == DRV_ULP_ALTO || condizione == DRV_ULP_BASSO;
  if (sorgente == DRV_ULP_ANALOGICO) return condizione >= DRV_ULP_SOPRA && condizione <= DRV_ULP_DENTRO;
  return false;
}

static bool controllaRigaN(const SogliaRiga& r, int numero, ErrBuf& err) {
  if (r.sorgente != DRV_ULP_LIVELLO && r.sorgente != DRV_ULP_ANALOGICO) {
    err.setf(tr("riga %d: sorgente non valida"), numero);
    return false;
  }
  if (r.piedino < 0) {
    err.setf(tr("riga %d: piedino non valido"), numero);
    return false;
  }
  if (!condizioneValida(r.sorgente, r.condizione)) {
    err.setf(tr("riga %d: condizione non valida per questa sorgente"), numero);
    return false;
  }
  if (r.campioni < 1 || r.campioni > SOGLIE_CAMPIONI_MAX) {
    err.setf(tr("riga %d: campioni fuori misura (1-255)"), numero);
    return false;
  }
  if (r.azione == 0 || (r.azione & ~(uint32_t)(DRV_ULP_SVEGLIA | DRV_ULP_REGISTRA))) {
    err.setf(tr("riga %d: azione non valida"), numero);
    return false;
  }
  if (r.sorgente == DRV_ULP_ANALOGICO) {
    if (r.soglia1 > SOGLIE_VALORE_MAX || r.soglia2 > SOGLIE_VALORE_MAX) {
      err.setf(tr("riga %d: soglie fuori misura (0-4095)"), numero);
      return false;
    }
    if (r.isteresi > SOGLIE_VALORE_MAX) {
      err.setf(tr("riga %d: isteresi fuori misura (0-4095)"), numero);
      return false;
    }
    bool banda = r.condizione == DRV_ULP_FUORI || r.condizione == DRV_ULP_DENTRO;
    if (banda && r.soglia1 > r.soglia2) {
      err.setf(tr("riga %d: la prima soglia deve essere minore o uguale alla seconda"), numero);
      return false;
    }
  }
  if (r.resistenza < DRV_ULP_RES_NESSUNA || r.resistenza > DRV_ULP_RES_GIU) {
    err.setf(tr("riga %d: resistenza non valida"), numero);
    return false;
  }
  return true;
}

bool sogliaControlla(const SogliaRiga& r, ErrBuf& err) {
  return controllaRigaN(r, 1, err);
}

// Traduce il codice del driver in un testo per l'utente.
static void testoErroreDriver(int codice, ErrBuf& err) {
  switch (codice) {
    case DRV_ULP_E_NON_DISPONIBILE: err = tr("il coprocessore non e disponibile su questa scheda"); break;
    case DRV_ULP_E_PIEDINO:         err = tr("piedino non adatto al coprocessore (serve un piedino RTC)"); break;
    case DRV_ULP_E_NON_ADC1:        err = tr("l'analogico deve stare su un piedino dell'ADC1"); break;
    case DRV_ULP_E_DUE_CANALI:      err = tr("per ora un solo piedino analogico"); break;
    case DRV_ULP_E_TROPPE_RIGHE:    err.setf(tr("troppe righe (massimo %d)"), drvUlpRigheMax()); break;
    case DRV_ULP_E_CARICAMENTO:     err = tr("il programma del coprocessore non e stato caricato"); break;
    case DRV_ULP_E_AVVIO:           err = tr("il coprocessore non si e avviato"); break;
    case DRV_ULP_E_ADC:             err = tr("l'ADC non si e preparato per il coprocessore"); break;
    default:                        err = tr("riga non valida"); break;
  }
}

bool soglieApplica(const SogliaRiga* righe, int n, uint32_t periodoMs, ErrBuf& err) {
  err.clear();
  if (!drvUlpDisponibile()) {
    testoErroreDriver(DRV_ULP_E_NON_DISPONIBILE, err);
    return false;
  }
  if (periodoMs < SOGLIE_PERIODO_MIN_MS || periodoMs > SOGLIE_PERIODO_MAX_MS) {
    err = tr("periodo fuori misura (10-60000 ms)");
    return false;
  }
  if (!righe || n < 0 || n > drvUlpRigheMax()) {
    testoErroreDriver(DRV_ULP_E_TROPPE_RIGHE, err);
    return false;
  }
  DrvUlpRiga dr[DRV_ULP_RIGHE_MAX];
  int attive = 0;
  for (int i = 0; i < n; i++) {
    if (!righe[i].attiva) continue;
    if (!controllaRigaN(righe[i], i + 1, err)) return false;
    DrvUlpRiga& d = dr[attive++];
    d.sorgente = righe[i].sorgente;
    d.piedino = righe[i].piedino;
    d.condizione = righe[i].condizione;
    d.soglia1 = righe[i].soglia1;
    d.soglia2 = righe[i].soglia2;
    d.isteresi = righe[i].isteresi;
    d.campioni = righe[i].campioni;
    d.azione = righe[i].azione;
    d.resistenza = righe[i].resistenza;
  }
  if (attive == 0) {
    drvUlpFerma();
    return true;
  }
  int e = drvUlpAvvia(dr, attive, periodoMs * 1000UL);
  if (e != DRV_ULP_OK) {
    testoErroreDriver(e, err);
    return false;
  }
  return true;
}

bool sogliaStato(int i, SogliaStato& s) {
  DrvUlpStato d;
  if (!drvUlpStato(i, d)) return false;
  s.valore = d.valore;
  s.vera = d.vera;
  s.scattata = d.scattata;
  s.cicli = d.cicli;
  return true;
}

void soglieFerma() {
  drvUlpFerma();
}
