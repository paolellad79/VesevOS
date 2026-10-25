// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_udp.cpp
#include "vos_drv_udp.h"
#include "vos_drv_task.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/sockets.h>

static const size_t PKT_MAX = 512;       // piu che sufficiente per NTP (48 byte) e per una domanda DNS

bool DrvUdp::begin(uint16_t port, DrvUdpHandler h, const char* taskName) {
  if (task_) return true;
  int s = lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (s < 0) return false;
  struct sockaddr_in a = {};
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_ANY);
  a.sin_port = htons(port);
  if (lwip_bind(s, (struct sockaddr*)&a, sizeof a) != 0) { lwip_close(s); return false; }
  struct timeval tv = { 0, 500000 };     // attesa a scatti di mezzo secondo, per accorgersi di end()
  lwip_setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
  sock_ = s; handler_ = h; stop_ = false;
  TaskHandle_t t = nullptr;
  if (!drvTaskCreate(run, taskName, 4096, this, 1, &t, DRV_CORE_ANY)) {   // core: automatico (prima: core 0 fisso)
    lwip_close(s); sock_ = -1; return false; }
  task_ = t;
  return true;
}

void DrvUdp::end() {
  if (!task_) return;
  stop_ = true;
  for (int i = 0; i < 40 && task_; i++) vTaskDelay(pdMS_TO_TICKS(50));   // il task si toglie da solo e azzera task_
}

void DrvUdp::run(void* self) {
  DrvUdp* u = (DrvUdp*)self;
  uint8_t in[PKT_MAX], out[PKT_MAX];
  while (!u->stop_) {
    struct sockaddr_in from;
    socklen_t fl = sizeof from;
    int n = lwip_recvfrom(u->sock_, in, sizeof in, 0, (struct sockaddr*)&from, &fl);
    if (n <= 0) continue;                // scaduto il tempo (o errore passeggero): si riprova
    size_t r = u->handler_(in, (size_t)n, out, sizeof out);
    if (r) lwip_sendto(u->sock_, out, r, 0, (struct sockaddr*)&from, fl);
  }
  lwip_close(u->sock_);
  u->sock_ = -1;
  u->task_ = nullptr;
  vTaskDelete(nullptr);
}
