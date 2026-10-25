// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_tcpsrv.cpp
#include "vos_drv_tcpsrv.h"
#include "vos_drv_task.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/sockets.h>
#include <errno.h>
#include <string.h>

static const int TASK_STACK = 4096;    // due buffer (richiesta e risposta, circa 1,8 KB) piu il lavoro del gestore; NON misurato sulla scheda
static const int TASK_PRIO  = 1;       // la piu bassa dopo l'idle: il redirect non toglie tempo a nessuno
static const int BACKLOG    = 3;       // chiamate in coda mentre se ne serve una; le altre vengono rifiutate da lwIP

bool DrvTcpMini::begin(uint16_t port, DrvTcpHandler h, const char* taskName) {
  if (task_) return true;
  int s = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s < 0) return false;
  int one = 1;
  lwip_setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
  struct sockaddr_in a = {};
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_ANY);
  a.sin_port = htons(port);
  if (lwip_bind(s, (struct sockaddr*)&a, sizeof a) != 0 || lwip_listen(s, BACKLOG) != 0) { lwip_close(s); return false; }
  sock_ = s; handler_ = h;
  TaskHandle_t t = nullptr;
  if (!drvTaskCreate(run, taskName, TASK_STACK, this, TASK_PRIO, &t, DRV_CORE_ANY)) {   // core: automatico, come tutti i task nostri (vedi vos_drv_task.h)
    lwip_close(s); sock_ = -1; return false; }
  task_ = t;
  return true;
}

void DrvTcpMini::run(void* self) {
  DrvTcpMini* d = (DrvTcpMini*)self;
  char in[TCPSRV_IN_MAX + 1], out[TCPSRV_OUT_MAX];
  for (;;) {
    struct sockaddr_in from;
    socklen_t fl = sizeof from;
    int c = lwip_accept(d->sock_, (struct sockaddr*)&from, &fl);
    if (c < 0) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }   // niente socket liberi o errore passeggero: si riprova senza girare a vuoto
    d->serve(c, ntohl(from.sin_addr.s_addr), in, out);
  }
}

// Legge la richiesta (fino alla riga vuota, a TCPSRV_IN_MAX byte o a TCPSRV_READ_MS), chiede la risposta al gestore, la invia e chiude.
void DrvTcpMini::serve(int c, uint32_t peerIp, char* in, char* out) {
  struct timeval step = { 0, 500000 };   // ogni lettura aspetta al massimo mezzo secondo, il tempo totale lo conta il ciclo
  lwip_setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &step, sizeof step);
  lwip_setsockopt(c, SOL_SOCKET, SO_SNDTIMEO, &step, sizeof step);
  size_t n = 0;
  TickType_t start = xTaskGetTickCount();
  while (n < TCPSRV_IN_MAX) {
    int r = lwip_recv(c, in + n, TCPSRV_IN_MAX - n, 0);
    if (r > 0) {
      n += (size_t)r;
      in[n] = 0;
      if (strstr(in, "\r\n\r\n") || strstr(in, "\n\n")) break;      // intestazioni finite
    } else if (r == 0 || (errno != EWOULDBLOCK && errno != EAGAIN)) break;   // chiuso da chi chiama o errore vero
    if ((xTaskGetTickCount() - start) * portTICK_PERIOD_MS > TCPSRV_READ_MS) break;   // troppo lento
  }
  in[n] = 0;
  bool lineDone = memchr(in, '\n', n) != nullptr || n >= TCPSRV_IN_MAX;   // almeno la prima riga e arrivata (o e troppo lunga)
  size_t len = lineDone ? handler_(peerIp, in, n, out, TCPSRV_OUT_MAX) : 0;
  for (size_t sent = 0; sent < len;) {
    int w = lwip_send(c, out + sent, len - sent, 0);
    if (w <= 0) break;
    sent += (size_t)w;
  }
  // Prima si chiude il verso di scrittura, poi si butta quello che chi chiama ha ancora da dire: chiudere con dati non letti manderebbe
  // un RST e il browser potrebbe perdere la risposta.
  lwip_shutdown(c, SHUT_WR);
  struct timeval quick = { 0, 100000 };
  lwip_setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &quick, sizeof quick);
  for (int i = 0; i < 4 && lwip_recv(c, in, TCPSRV_IN_MAX, 0) > 0; i++) {}
  lwip_close(c);
}
