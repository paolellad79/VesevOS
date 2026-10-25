// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_resolve.cpp
// Il lavoro (struct Job) sta sull'heap e ha DUE proprietari: chi chiama e il task di aiuto. Se chi chiama non aspetta piu (scadenza),
// il task finisce quando getaddrinfo risponde o si arrende da solo (i tentativi di lwIP hanno un tempo massimo) e libera lui il lavoro.
#include "vos_drv_resolve.h"
#include "vos_drv_task.h"
#include "freertos/semphr.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include <stdlib.h>
#include <string.h>

namespace {

const uint32_t kHelperStack = 4096;
const UBaseType_t kHelperPrio = 3;

struct Job {
  char              host[64];
  uint32_t          ip;
  bool              ok;
  int               owners;      // 2 all'inizio; chi lascia per ultimo libera
  SemaphoreHandle_t done;
};

void unref(Job* j) {
  if (__atomic_sub_fetch(&j->owners, 1, __ATOMIC_ACQ_REL) != 0) return;
  vSemaphoreDelete(j->done);
  free(j);
}

void helper(void* arg) {
  Job* j = (Job*)arg;
  struct addrinfo hints;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo* res = nullptr;
  if (getaddrinfo(j->host, nullptr, &hints, &res) == 0 && res) {
    if (res->ai_family == AF_INET && res->ai_addr) {
      j->ip = ntohl(((struct sockaddr_in*)res->ai_addr)->sin_addr.s_addr);     // da rete a "primo numero in alto"
      j->ok = j->ip != 0;
    }
  }
  if (res) freeaddrinfo(res);
  xSemaphoreGive(j->done);
  unref(j);
  vTaskDelete(nullptr);
}

}  // namespace

bool drvDnsResolve(const char* host, uint32_t timeoutMs, uint32_t& ip) {
  if (!host || !*host || strlen(host) >= sizeof(((Job*)nullptr)->host)) return false;
  Job* j = (Job*)calloc(1, sizeof(Job));
  if (!j) return false;
  j->done = xSemaphoreCreateBinary();
  if (!j->done) { free(j); return false; }
  strcpy(j->host, host);
  j->owners = 2;
  if (!drvTaskCreate(helper, "dnsq", kHelperStack, j, kHelperPrio, nullptr, DRV_CORE_ANY)) {     // core: automatico
    vSemaphoreDelete(j->done);
    free(j);
    return false;
  }
  bool got = xSemaphoreTake(j->done, pdMS_TO_TICKS(timeoutMs)) == pdTRUE && j->ok;
  if (got) ip = j->ip;
  unref(j);
  return got;
}
