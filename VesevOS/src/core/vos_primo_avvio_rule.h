// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_primo_avvio_rule.h
// Regola della PRIMA ACCENSIONE, senza dipendenze (cosi si prova sul computer).
// Tre stati:
//   NORMALE : la scheda e configurata (guida finita): tutto come sempre.
//   ATTESA  : scheda non configurata, radio SPENTA di proposito, si aspetta una pressione del tasto BOOT.
//   FINESTRA: dopo la pressione, per 10 minuti, hotspot aperto con HTTPS, portale automatico (DNS) e redirect HTTP (decisione del 15/11/2026).
// Transizioni:
//   avvio con guida non finita -> ATTESA;   avvio con guida finita -> NORMALE
//   ATTESA --pressione--> FINESTRA (evento APERTA);   FINESTRA --pressione--> FINESTRA (rinnova i 10 minuti)
//   FINESTRA --10 minuti senza client collegati e senza richieste--> ATTESA (evento SCADUTA)
//   FINESTRA, con un client collegato oppure una richiesta con accesso: i 10 minuti ricominciano
//   ATTESA o FINESTRA --guida finita--> NORMALE (evento FINITA, dato una volta sola, PA_RITARDO_FINE_MS dopo la fine:
//   la pagina deve fare in tempo a mostrare "Configurazione completata" prima che l'hotspot passi a protetto)
// Chi scrive: il tasto (paPressione, ciclo principale), la guida (paFine, pagina o seriale), il task di rete (paPasso).
// Gli scambi sono semplici (parole da 32 bit): in paPressione l'ora di fine si scrive PRIMA dello stato, cosi il task di rete
// non vede mai una finestra aperta con la scadenza vecchia.
#pragma once
#include <stdint.h>

enum PaStato : uint8_t { PA_NORMALE = 0, PA_ATTESA, PA_FINESTRA };
enum PaEvento : uint8_t { PAE_NESSUNO = 0, PAE_APERTA, PAE_SCADUTA, PAE_FINITA };

static const uint32_t PA_FINESTRA_MS = 600000UL;   // 10 minuti dopo la pressione
static const uint32_t PA_RITARDO_FINE_MS = 4000UL; // tra la fine della guida e il passaggio dell'hotspot a protetto

struct PaCtx {
  volatile PaStato st = PA_NORMALE;
  volatile uint32_t fine = 0;          // ora (ms) in cui la finestra scade
  volatile bool finitaPendente = false;   // la guida e finita ma il task di rete non lo ha ancora saputo
  volatile uint32_t finitaOra = 0;        // ora (ms) dalla quale il task di rete puo saperlo
};

static inline void paInizia(PaCtx& c, bool guidaFinita) {
  c.fine = 0;
  c.finitaOra = 0;
  c.finitaPendente = false;
  c.st = guidaFinita ? PA_NORMALE : PA_ATTESA;
}

// Pressione breve del tasto. APERTA se ha acceso la finestra adesso, NESSUNO se era gia aperta (rinnovata) o se la scheda e configurata.
static inline PaEvento paPressione(PaCtx& c, uint32_t adesso) {
  if (c.st == PA_NORMALE) return PAE_NESSUNO;
  bool eraAperta = (c.st == PA_FINESTRA);
  c.fine = adesso + PA_FINESTRA_MS;
  c.st = PA_FINESTRA;
  return eraAperta ? PAE_NESSUNO : PAE_APERTA;
}

// Una richiesta con accesso durante la guida: i 10 minuti ricominciano (solo con la finestra aperta).
static inline void paAttivita(PaCtx& c, uint32_t adesso) {
  if (c.st == PA_FINESTRA) c.fine = adesso + PA_FINESTRA_MS;
}

// La guida e finita (pagina o seriale): subito NORMALE; il task di rete avra l'evento FINITA dopo 'ritardoMs'.
static inline void paFine(PaCtx& c, uint32_t adesso, uint32_t ritardoMs) {
  if (c.st == PA_NORMALE) return;
  c.finitaOra = adesso + ritardoMs;               // prima l'ora, poi il flag (come in paPressione)
  c.st = PA_NORMALE;
  c.finitaPendente = true;
}

// Un passo al secondo, dal task di rete. 'guidaFinita' e cfg.setupDone (prende anche le guide chiuse da altre strade).
// 'clienti' = dispositivi collegati all'hotspot adesso (basta contarli con la finestra aperta): finche ce n'e uno la finestra non scade.
static inline PaEvento paPasso(PaCtx& c, uint32_t adesso, bool guidaFinita, int clienti = 0) {
  if (c.finitaPendente) {
    if ((int32_t)(c.finitaOra - adesso) > 0) return PAE_NESSUNO;
    c.finitaPendente = false;
    return PAE_FINITA;
  }
  if (c.st == PA_NORMALE) return PAE_NESSUNO;
  if (guidaFinita) {                              // guida chiusa da un'altra strada (o cfg.setupDone scritto un attimo prima di paFine):
    c.finitaOra = adesso + PA_RITARDO_FINE_MS;    // anche qui il ritardo, mai un evento immediato (la risposta della pagina deve arrivare)
    c.st = PA_NORMALE;
    c.finitaPendente = true;
    return PAE_NESSUNO;
  }
  if (c.st == PA_FINESTRA && (int32_t)(c.fine - adesso) <= 0) {
    if (clienti > 0) { c.fine = adesso + PA_FINESTRA_MS; return PAE_NESSUNO; }
    c.st = PA_ATTESA;
    return PAE_SCADUTA;
  }
  return PAE_NESSUNO;
}

// Cosa deve essere acceso in ogni stato (una sola tabella: chi avvia i servizi la consulta).
static inline bool paRadioAccesa(PaStato s)    { return s != PA_ATTESA; }
static inline bool paServiziMinimi(PaStato s)  { return s != PA_NORMALE; }   // niente mDNS, password e percorsi oltre la guida (il portale automatico e il redirect HTTP ci sono: 15/11/2026)

// Fine guida (decisione dell'utente del 14/11/2026): con la Wi-Fi di casa configurata il punto di accesso, il portale automatico
// e il DHCP restano SPENTI; l'utente li accende dalla pagina (Rete/Servizi). Senza Wi-Fi di casa non si tocca niente
// (la scheda resterebbe irraggiungibile). true = ha spento qualcosa.
static inline bool paServiziFineGuida(bool wifiCasaConfigurata, bool& apOn, bool& apCaptive, bool& dhcpOn) {
  if (!wifiCasaConfigurata) return false;
  bool cambiato = apOn || apCaptive || dhcpOn;
  apOn = false;
  apCaptive = false;
  dhcpOn = false;                        // il DHCP serve solo all'hotspot: segue lui
  return cambiato;
}
