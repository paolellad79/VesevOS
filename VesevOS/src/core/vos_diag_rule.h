// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_diag_rule.h
// Regole PURE della diagnostica con i lampi del LED (nessuna dipendenza dal chip: si provano sul computer).
// Un codice e una coppia (classe 1-9, dettaglio 1-5), scritta D<classe><nn>: classe 3, dettaglio 1 = D301.
// Il LED mostra SEMPRE classe e dettaglio (decisione dell'utente del 16/11/2026, scelta B):
//   - nessun lampo di inizio: l'inizio si riconosce dalla pausa di ripetizione (3000 ms), piu lunga di quella tra i gruppi (1200 ms);
//   - lampo corto 150 ms acceso + 183 ms spento (3 al secondo); lampo lungo 700/500 ms;
//   - le pause sono IN PIU rispetto ai 183 ms di spento dell'ultimo lampo del gruppo;
//   - codice sconosciuto (riservato) = 3 lampi lunghi.
// Qui stanno SOLO gli errori che il firmware rileva davvero (documento build/analisi/diagnostica-led-2026-11-16.md, sezione 3).
#pragma once
#include <stdint.h>

enum DiagClasse : uint8_t {
  DC_AVVIO = 1, DC_CPU = 2, DC_WATCHDOG = 3, DC_MEMORIA = 4, DC_FLASH = 5, DC_PERIF = 6, DC_ALIM = 7, DC_RETE = 8, DC_APP = 9
};
static const uint8_t DIAG_CLASSI = 9;
static const uint8_t DIAG_DETT_MAX = 5;

enum DiagGrav : uint8_t { DGR_INFO = 0, DGR_RECUP = 1, DGR_DEGRADO = 2, DGR_CRITICO = 3, DGR_FATALE = 4 };
enum DiagQuando : uint8_t { DQU_MAI = 0, DQU_AVVIO = 1, DQU_PERSISTE = 2 };   // AVVIO = dopo un riavvio anomalo; PERSISTE = finche dura

struct DiagCodice { uint8_t classe, dett; };
static const DiagCodice DIAG_NESSUNO   = { 0, 0 };
static const DiagCodice DIAG_RISERVATO = { 255, 255 };     // errore sconosciuto: 3 lampi lunghi

struct DiagVoce { uint8_t classe, dett, grav, quando; const char* nome; };

// La tabella sta in vos_diag_tab.cpp (i testi li legge lo scanner delle traduzioni, che guarda solo i .cpp). Un numero tolto non si riusa mai.
const DiagVoce* diagTabella(int* n);

static inline bool diagUguali(DiagCodice a, DiagCodice b) { return a.classe == b.classe && a.dett == b.dett; }
static inline bool diagNessuno(DiagCodice c) { return c.classe == 0 && c.dett == 0; }
static inline bool diagRiservato(DiagCodice c) { return diagUguali(c, DIAG_RISERVATO); }

static inline const DiagVoce* diagVoce(DiagCodice c) {
  int n = 0;
  const DiagVoce* t = diagTabella(&n);
  for (int i = 0; i < n; i++) {
    if (t[i].classe == c.classe && t[i].dett == c.dett) return &t[i];
  }
  return nullptr;
}

// Un codice che non e in tabella (classe o dettaglio fuori) diventa il codice riservato. {0,0} resta "nessun codice".
static inline DiagCodice diagNormalizza(DiagCodice c) {
  if (diagNessuno(c)) return c;
  return diagVoce(c) ? c : DIAG_RISERVATO;
}

// Il codice riservato conta come degrado che si mostra dopo il riavvio.
static inline uint8_t diagGravita(DiagCodice c) {
  const DiagVoce* v = diagVoce(c);
  if (v) return v->grav;
  return diagRiservato(c) ? (uint8_t)DGR_DEGRADO : (uint8_t)DGR_INFO;
}

static inline uint8_t diagQuando(DiagCodice c) {
  const DiagVoce* v = diagVoce(c);
  if (v) return v->quando;
  return diagRiservato(c) ? (uint8_t)DQU_AVVIO : (uint8_t)DQU_MAI;
}

// Con il LED spento dalle impostazioni, un guasto fatale o critico lampeggia COMUNQUE (decisione dell'utente del 16/11/2026).
static inline bool diagMostra(DiagCodice c, bool ledSpento) {
  if (diagNessuno(c) || diagQuando(c) == DQU_MAI) return false;
  return !ledSpento || diagGravita(c) >= DGR_CRITICO;
}

// Codice dell'ultimo riavvio: lo scrive vos_diag.cpp all'avvio, lo legge il diario. Sta qui (e non in vos_diag.h) per non chiudere un ciclo tra moduli.
// 'inline' (non 'static'): la variabile e una sola per tutto il programma.
inline DiagCodice& diagCodiceAvvio() {
  static DiagCodice c = {0, 0};
  return c;
}

// ---- Causa del riavvio (valori di esp_reset_reason_t, IDF 5.5.5, esp_system.h) ----
enum DiagReset : int {
  DR_SCONOSCIUTO = 0, DR_ACCENSIONE = 1, DR_ESTERNO = 2, DR_SW = 3, DR_PANICO = 4, DR_INT_WDT = 5, DR_TASK_WDT = 6, DR_WDT = 7,
  DR_SONNO = 8, DR_BROWNOUT = 9, DR_SDIO = 10, DR_USB = 11, DR_JTAG = 12, DR_EFUSE = 13, DR_PWR_GLITCH = 14, DR_CPU_LOCKUP = 15
};

// Dalla causa del riavvio al codice. 'dalPanico' e il codice salvato dal gancio del panico (DIAG_NESSUNO se non c'e): serve solo per DR_PANICO.
// Sull'S3 il codice per DR_SDIO non esiste: il valore e di C6 e si tratta come gli altri errori di alimentazione.
static inline DiagCodice diagDaReset(int r, DiagCodice dalPanico) {
  switch (r) {
    case DR_PANICO:     return diagNessuno(dalPanico) ? DiagCodice{DC_CPU, 2} : diagNormalizza(dalPanico);
    case DR_TASK_WDT:   return DiagCodice{DC_WATCHDOG, 1};
    case DR_INT_WDT:    return DiagCodice{DC_WATCHDOG, 2};
    case DR_WDT:        return DiagCodice{DC_WATCHDOG, 3};
    case DR_BROWNOUT:   return DiagCodice{DC_ALIM, 1};
    case DR_SDIO:
    case DR_EFUSE:
    case DR_PWR_GLITCH: return DiagCodice{DC_ALIM, 2};
    case DR_ACCENSIONE: case DR_ESTERNO: case DR_SW: case DR_SONNO: case DR_USB: case DR_JTAG:
      return DIAG_NESSUNO;
    default:            return DIAG_RISERVATO;     // sconosciuto (0), blocco della CPU (15) o un valore nuovo
  }
}

// ---- Dal panico al codice (dentro il panico: niente memoria, niente funzioni di libreria) ----
// 'tipo' = panic_exception_t di ESP-IDF 5.5.5 (panic_internal.h): DEBUG 0, IWDT 1, TWDT 2, ABORT 3, FAULT 4.
enum DiagPanicoTipo : int { DPK_DEBUG = 0, DPK_IWDT = 1, DPK_TWDT = 2, DPK_ABORT = 3, DPK_FAULT = 4 };

// vero se 'ago' sta dentro 'testo' (confronto semplice, sicuro nel panico)
static inline bool diagContiene(const char* testo, const char* ago) {
  if (!testo || !ago || !*ago) return false;
  for (const char* t = testo; *t; t++) {
    const char* a = ago;
    const char* p = t;
    while (*a && *p == *a) { a++; p++; }
    if (!*a) return true;
  }
  return false;
}

// 'seg' = segnali letti dal gancio del panico (drivers/vos_drv_panic.cpp). In ESP-IDF 5.5.5 (letto nei sorgenti) il panico del Task watchdog
// NON ha un tipo suo: PANIC_EXCEPTION_TWDT non e mai assegnato, il gestore lo ritrasforma in "abort" e il motivo e quello del task fermato
// (spesso "Syscall"). L'unico segno affidabile e il flag g_twdt_isr di task_wdt.c, messo a vero SOLO da task_wdt_timeout_abort().
// Per la pila: FreeRTOS (canary) chiama abort con "***ERROR*** A stack overflow in task X has been detected." (port.c); sull'S3 il punto di
// controllo di fine pila e una eccezione di debug con DEBUGCAUSE bit DBREAK e bit 8 (panic_arch.c, print_debug_exception_details).
enum DiagPanicoSegni : uint32_t { DPS_TWDT = 1, DPS_PILA_PUNTO = 2 };

static inline DiagCodice diagDaPanico(int tipo, const char* motivo, uint32_t seg = 0, const char* dettagli = nullptr) {
  if (tipo == DPK_IWDT) return DiagCodice{DC_WATCHDOG, 2};
  if (tipo == DPK_TWDT || (seg & DPS_TWDT)) return DiagCodice{DC_WATCHDOG, 1};
  if ((seg & DPS_PILA_PUNTO) || diagContiene(dettagli, "stack overflow in task")) return DiagCodice{DC_MEMORIA, 1};
  if (diagContiene(motivo, "Stack canary") || diagContiene(motivo, "Stack protection fault")) return DiagCodice{DC_MEMORIA, 1};
  if (diagContiene(motivo, "Cache error")) return DiagCodice{DC_CPU, 3};
  if (diagContiene(motivo, "Memory protection")) return DiagCodice{DC_CPU, 4};
  if (tipo == DPK_DEBUG) return DiagCodice{DC_CPU, 5};
  return DiagCodice{DC_CPU, 2};                    // eccezione, abort o altro: causa generica
}

// ---- La sequenza dei lampi ----
static const uint16_t DIAG_CORTO_ON   = 150;
static const uint16_t DIAG_CORTO_OFF  = 183;       // periodo 333 ms = 3 lampi al secondo
static const uint16_t DIAG_LUNGO_ON   = 700;
static const uint16_t DIAG_LUNGO_OFF  = 500;
static const uint16_t DIAG_PAUSA_GRUPPI  = 1200;   // tra classe e dettaglio (in piu dello spento del lampo)
static const uint16_t DIAG_PAUSA_RIPETI  = 3000;   // tra un giro e il successivo
static const uint8_t  DIAG_LUNGHI_RISERVATO = 3;

struct DiagPasso { uint16_t acceso, spento; };

// Quanti lampi ha il codice: classe + dettaglio (riservato: 3 lunghi). Classe 1-9 e dettaglio 1-5 (anche fuori tabella: chi mostra
// un codice lo normalizza prima con diagNormalizza); altro e {0,0}: zero.
static inline int diagNumPassi(DiagCodice c) {
  if (diagRiservato(c)) return DIAG_LUNGHI_RISERVATO;
  if (c.classe < 1 || c.classe > DIAG_CLASSI || c.dett < 1 || c.dett > DIAG_DETT_MAX) return 0;
  return (int)c.classe + (int)c.dett;
}

// Il passo i (0 = primo lampo). 'ripeti' = giro che si ripete (pausa di 3000 ms in coda); falso = giro compatto (nel panico: nessuna pausa in coda).
static inline DiagPasso diagPasso(DiagCodice c, int i, bool ripeti) {
  DiagPasso p = {0, 0};
  int n = diagNumPassi(c);
  if (i < 0 || i >= n) return p;
  if (diagRiservato(c)) {
    p.acceso = DIAG_LUNGO_ON;
    p.spento = DIAG_LUNGO_OFF;
  } else {
    p.acceso = DIAG_CORTO_ON;
    p.spento = DIAG_CORTO_OFF;
    if (i == (int)c.classe - 1) p.spento = (uint16_t)(p.spento + DIAG_PAUSA_GRUPPI);
  }
  if (i == n - 1) {
    if (ripeti) p.spento = (uint16_t)(p.spento + DIAG_PAUSA_RIPETI);
    else p.spento = 0;
  }
  return p;
}

// Durata di un giro in millisecondi (0 se il codice non ha lampi)
static inline uint32_t diagDurataGiro(DiagCodice c, bool ripeti) {
  uint32_t t = 0;
  int n = diagNumPassi(c);
  for (int i = 0; i < n; i++) {
    DiagPasso p = diagPasso(c, i, ripeti);
    t += (uint32_t)p.acceso + p.spento;
  }
  return t;
}

// Il LED e acceso al millisecondo 'ms' di un giro che si ripete? ms puo superare la durata (si riparte dall'inizio).
static inline bool diagAccesoA(DiagCodice c, uint32_t ms) {
  uint32_t durata = diagDurataGiro(c, true);
  if (!durata) return false;
  ms %= durata;
  int n = diagNumPassi(c);
  for (int i = 0; i < n; i++) {
    DiagPasso p = diagPasso(c, i, true);
    if (ms < p.acceso) return true;
    ms -= p.acceso;
    if (ms < p.spento) return false;
    ms -= p.spento;
  }
  return false;
}

// Colore consigliato sul LED a colori (solo un aiuto: il conteggio dei lampi basta): rosso fatale/critico, arancio degrado, giallo il resto
static inline void diagColore(DiagCodice c, uint8_t* r, uint8_t* g, uint8_t* b) {
  uint8_t gr = diagGravita(c);
  *b = 0;
  if (gr >= DGR_CRITICO) { *r = 255; *g = 0; }
  else if (gr == DGR_DEGRADO) { *r = 255; *g = 110; }
  else { *r = 255; *g = 200; }
}

// Quanto dura al massimo il giro compatto nel panico (classe 4 + dettaglio 5, senza pausa in coda): 9 lampi corti + pausa tra i gruppi - ultimo spento
static const uint32_t DIAG_PANICO_MAX_MS = 9u * (DIAG_CORTO_ON + DIAG_CORTO_OFF) + DIAG_PAUSA_GRUPPI - DIAG_CORTO_OFF;
