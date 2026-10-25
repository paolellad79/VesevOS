// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_ping.cpp
// Come funziona: esp_ping_new_session crea un task di lwIP che invia i pacchetti e chiama, nel SUO task, tre funzioni (risposta, mancata
// risposta, fine). Queste scrivono solo in "g" (una sola sessione alla volta, quindi un solo blocco statico) e il task di chi chiama le
// legge e le consegna a onSample. Cosi la stampa avviene sempre nel task del comando, mai in quello di lwIP.
#include "vos_drv_ping.h"
#include "vos_drv_task.h"
#include "esp_timer.h"
#include "freertos/semphr.h"
#include "ping/ping_sock.h"
#include "lwip/ip_addr.h"
#include <math.h>
#include <string.h>

namespace {

const uint32_t kMarginMs = 1000;        // margine della scadenza di sicurezza
const uint32_t kEndWaitMs = 500;        // dopo la richiesta di stop: quanto aspettare la fine in piu dell'attesa di un pacchetto
const uint32_t kPingStack = 3072;
const uint32_t kPingPrio = 2;

// Lo stato della sessione. Lo scrive il task del ping (campioni, fine); lo legge chi chiama. I contatori passano per le operazioni
// atomiche: il task del ping puo stare sull'altro core.
struct Session {
  DrvPingSample samples[DRV_PING_COUNT_MAX];
  uint32_t filled;               // quanti campioni sono scritti (il campione i e valido quando i < filled)
  uint32_t count;                // pacchetti previsti
  uint32_t sent, received;       // scritti alla fine dal task del ping
  int64_t  startUs, lastUs;      // avvio e conclusione dell'ultimo pacchetto
  bool     ended;                // il task del ping ha chiamato la funzione "fine"
  bool     abandoned;            // chi chiama se n'e andato: sara la funzione "fine" a liberare la sessione
  bool     released;             // la sessione e stata liberata (una volta sola)
  bool     finishing;            // tutti i pacchetti sono arrivati: manca solo l'ultima pausa del task del ping
};

Session g;
int g_busy = 0;                  // 1 = sessione in corso
SemaphoreHandle_t g_endSem = nullptr;

// SEQ_CST: tra "fine" e "chi chiama" deve vincere sempre uno dei due nel liberare la sessione (nessun caso in cui nessuno lo fa)
bool ld(const bool& v) { return __atomic_load_n(&v, __ATOMIC_SEQ_CST); }
void st(bool& v, bool x) { __atomic_store_n(&v, x, __ATOMIC_SEQ_CST); }

// La sessione e finita per davvero: la si libera, ma una volta sola (chi arriva primo, tra chiamante e funzione "fine").
void release() {
  if (!__atomic_exchange_n(&g.released, true, __ATOMIC_ACQ_REL)) __atomic_store_n(&g_busy, 0, __ATOMIC_RELEASE);
}

// Prende il posto della sessione. Se la sessione di prima ha gia consegnato tutti i pacchetti manca solo l'ultima pausa
// dell'intervallo del suo task: si aspetta fino a 3 secondi, senza mai bloccarsi di piu.
bool acquire() {
  for (int i = 0; i < 60; i++) {
    if (__atomic_exchange_n(&g_busy, 1, __ATOMIC_ACQ_REL) == 0) return true;
    if (!ld(g.finishing)) return false;
    vTaskDelay(pdMS_TO_TICKS(50));
  }
  return false;
}

// ---------- le tre funzioni chiamate dal task del ping ----------

// Un pacchetto e concluso: risposta (ok) o mancata risposta. Scrive il campione e poi alza "filled".
void storeSample(esp_ping_handle_t hdl, bool ok) {
  uint32_t i = __atomic_load_n(&g.filled, __ATOMIC_RELAXED);
  if (i >= DRV_PING_COUNT_MAX) return;
  DrvPingSample& s = g.samples[i];
  memset(&s, 0, sizeof s);
  uint16_t seq = 0;
  esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seq, sizeof seq);
  s.seq = seq;
  s.ok = ok;
  if (ok) {
    uint8_t ttl = 0;
    uint32_t data = 0, ms = 0;
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof ttl);
    esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &data, sizeof data);       // i soli dati (senza le 8 byte di intestazione ICMP)
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &ms, sizeof ms);
    s.ttl = ttl;
    s.size = (uint16_t)(data + 8);
    s.us = ms * 1000u;
  }
  g.lastUs = esp_timer_get_time();
  __atomic_store_n(&g.filled, i + 1, __ATOMIC_RELEASE);
}

void onSuccess(esp_ping_handle_t hdl, void*) { storeSample(hdl, true); }
void onTimeout(esp_ping_handle_t hdl, void*) { storeSample(hdl, false); }

// Fine della sessione: si salvano i contatori, si chiede a lwIP di chiudere il task (libera da solo socket e memoria) e si avvisa chi aspetta.
void onEnd(esp_ping_handle_t hdl, void*) {
  uint32_t sent = 0, received = 0;
  esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &sent, sizeof sent);
  esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof received);
  g.sent = sent;
  g.received = received;
  esp_ping_delete_session(hdl);
  st(g.ended, true);
  xSemaphoreGive(g_endSem);
  if (ld(g.abandoned)) release();          // chi chiamava non c'e piu: l'ultima cosa che si fa, senza toccare altro in "g" dopo
}

// ---------- chi chiama ----------

// Consegna a onSample i campioni nuovi (da "done" in poi). Ritorna il nuovo "done".
uint32_t deliver(uint32_t done, DrvPingSampleFn onSample, void* user) {
  uint32_t n = __atomic_load_n(&g.filled, __ATOMIC_ACQUIRE);
  for (; done < n; done++)
    if (onSample) onSample(g.samples[done], user);
  return done;
}

// Minimo, media, massimo e deviazione (come "mdev" di ping) dei pacchetti con risposta, in microsecondi.
void rtt(DrvPingResult& r, uint32_t filled) {
  uint32_t n = 0;
  uint64_t sum = 0, sum2 = 0;
  for (uint32_t i = 0; i < filled; i++) {
    if (!g.samples[i].ok) continue;
    uint32_t v = g.samples[i].us;
    if (n == 0 || v < r.minUs) r.minUs = v;
    if (v > r.maxUs) r.maxUs = v;
    sum += v;
    sum2 += (uint64_t)v * v;
    n++;
  }
  if (!n) return;
  double avg = (double)sum / n;
  double var = (double)sum2 / n - avg * avg;
  r.avgUs = (uint32_t)(avg + 0.5);
  r.mdevUs = var > 0 ? (uint32_t)(sqrt(var) + 0.5) : 0;
}

bool argsOk(const DrvPingReq& q) {
  return q.ip != 0 && q.count >= 1 && q.count <= DRV_PING_COUNT_MAX
         && q.intervalMs >= DRV_PING_INTERVAL_MIN_MS && q.intervalMs <= DRV_PING_INTERVAL_MAX_MS
         && q.timeoutMs >= DRV_PING_TIMEOUT_MIN_MS && q.timeoutMs <= DRV_PING_TIMEOUT_MAX_MS
         && q.dataSize >= DRV_PING_SIZE_MIN && q.dataSize <= DRV_PING_SIZE_MAX && q.ttl >= 1 && q.ttl <= 255;
}

}  // namespace

DrvPingErr drvPingRun(const DrvPingReq& req, DrvPingSampleFn onSample, DrvPingCancelFn cancel, void* user, DrvPingResult& out) {
  memset(&out, 0, sizeof out);
  if (!argsOk(req)) return DP_BAD_ARG;
  if (!g_endSem) {
    g_endSem = xSemaphoreCreateBinary();
    if (!g_endSem) return DP_NO_MEM;
  }
  if (!acquire()) return DP_BUSY;

  memset(&g, 0, sizeof g);
  g.count = req.count;
  while (xSemaphoreTake(g_endSem, 0) == pdTRUE) {}           // niente segnali rimasti da una sessione di prima

  esp_ping_config_t cfg = ESP_PING_DEFAULT_CONFIG();
  cfg.count = req.count;
  cfg.interval_ms = req.intervalMs;
  cfg.timeout_ms = req.timeoutMs;
  cfg.data_size = req.dataSize;
  cfg.ttl = (int)req.ttl;
  cfg.task_stack_size = kPingStack;
  cfg.task_prio = kPingPrio;
  IP_ADDR4(&cfg.target_addr, (req.ip >> 24) & 0xFF, (req.ip >> 16) & 0xFF, (req.ip >> 8) & 0xFF, req.ip & 0xFF);

  esp_ping_callbacks_t cbs = {};
  cbs.on_ping_success = onSuccess;
  cbs.on_ping_timeout = onTimeout;
  cbs.on_ping_end = onEnd;

  esp_ping_handle_t hdl = nullptr;
  if (esp_ping_new_session(&cfg, &cbs, &hdl) != ESP_OK || !hdl) { release(); return DP_NO_MEM; }
  g.startUs = esp_timer_get_time();
  g.lastUs = g.startUs;
  if (esp_ping_start(hdl) != ESP_OK) {
    esp_ping_delete_session(hdl);                            // il task di lwIP si chiude da solo; nessuna funzione "fine" arrivera
    release();
    return DP_START;
  }

  // Attesa: a scatti di 100 ms. Scadenza di sicurezza = conteggio x intervallo + attesa + margine. Il watchdog dei task si alimenta a ogni giro.
  const int64_t deadlineUs = g.startUs + (int64_t)(req.count * req.intervalMs + req.timeoutMs + kMarginMs) * 1000;
  uint32_t done = 0;
  bool stopped = false;
  while (!ld(g.ended)) {
    if (xSemaphoreTake(g_endSem, pdMS_TO_TICKS(100)) == pdTRUE) break;
    drvTaskPause();
    done = deliver(done, onSample, user);
    if (done >= req.count) { st(g.finishing, true); break; }                  // tutto consegnato: non si aspetta l'ultima pausa dell'intervallo
    if (cancel && cancel(user)) { out.cancelled = true; stopped = true; break; }
    if (esp_timer_get_time() > deadlineUs) { out.timedOut = true; stopped = true; break; }
  }
  if (stopped) {
    esp_ping_stop(hdl);                                      // il task del ping finisce il pacchetto in corso (al massimo l'attesa) e chiama "fine"
    for (uint32_t waited = 0; !ld(g.ended) && waited < req.timeoutMs + kEndWaitMs; waited += 50) {
      vTaskDelay(pdMS_TO_TICKS(50));
      drvTaskPause();
    }
  }
  done = deliver(done, onSample, user);

  uint32_t filled = __atomic_load_n(&g.filled, __ATOMIC_ACQUIRE);
  out.sent = ld(g.ended) ? g.sent : filled;                  // senza "fine" ancora: i pacchetti di cui si conosce l'esito
  out.received = 0;
  for (uint32_t i = 0; i < filled; i++) out.received += g.samples[i].ok ? 1u : 0u;
  if (out.sent < filled) out.sent = filled;
  out.totalMs = (uint32_t)((g.lastUs - g.startUs) / 1000);
  rtt(out, filled);

  // Liberazione: se la funzione "fine" e arrivata si libera qui; altrimenti la lascia a lei (finira, perche ogni attesa ha il suo tempo massimo)
  st(g.abandoned, true);
  if (ld(g.ended)) release();
  return DP_OK;
}
