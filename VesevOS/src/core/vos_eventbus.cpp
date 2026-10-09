// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_eventbus.cpp
#include "vos_eventbus.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct SubSlot { char prefix[16]; EventBus::Handler h; };
static Event     g_ring[EventBus::CAP];
static uint32_t  g_seq = 0;
static SubSlot   g_sub[EventBus::MAXSUB];
static SemaphoreHandle_t g_mx = nullptr;
static volatile uint8_t  g_depth = 0;        // anti-ricorsione: un gestore che pubblica non fa partire altri gestori

static void lockEb()   { if (!g_mx) g_mx = xSemaphoreCreateMutex(); if (g_mx) xSemaphoreTake(g_mx, portMAX_DELAY); }
static void unlockEb() { if (g_mx) xSemaphoreGive(g_mx); }

// copia solo caratteri sicuri (cosi il JSON non ha bisogno di escape)
static void safeCopy(char* dst, size_t n, const char* src) {
  size_t i = 0;
  if (src) for (; src[i] && i + 1 < n; i++) { char c = src[i]; dst[i] = (c >= 32 && c < 127 && c != '"' && c != '\\') ? c : '_'; }
  dst[i] = 0;
}

void EventBus::publish(const char* topic, const char* arg, int32_t val) {
  if (!topic || !topic[0]) return;
  Event e; e.ms = millis(); e.val = val;
  safeCopy(e.topic, sizeof(e.topic), topic); safeCopy(e.arg, sizeof(e.arg), arg);
  lockEb();
  e.seq = ++g_seq; if (e.seq == 0) e.seq = ++g_seq;
  g_ring[e.seq % CAP] = e;
  SubSlot subs[MAXSUB]; memcpy(subs, g_sub, sizeof(subs));   // gestori chiamati FUORI dal blocco
  unlockEb();
  if (g_depth > 0) return;
  g_depth++;
  for (int i = 0; i < MAXSUB; i++)
    if (subs[i].h && strncmp(e.topic, subs[i].prefix, strlen(subs[i].prefix)) == 0) subs[i].h(e);
  g_depth--;
}

bool EventBus::subscribe(const char* prefix, Handler h) {
  if (!h) return false;
  char p[16]; safeCopy(p, sizeof(p), prefix ? prefix : "");
  bool okk = false;
  lockEb();
  int freeI = -1;
  for (int i = 0; i < MAXSUB; i++) {
    if (g_sub[i].h == h && strcmp(g_sub[i].prefix, p) == 0) { unlockEb(); return false; }
    if (!g_sub[i].h && freeI < 0) freeI = i;
  }
  if (freeI >= 0) { strcpy(g_sub[freeI].prefix, p); g_sub[freeI].h = h; okk = true; }
  unlockEb();
  return okk;
}

bool EventBus::unsubscribe(Handler h) {
  bool any = false;
  lockEb();
  for (int i = 0; i < MAXSUB; i++) if (g_sub[i].h == h && h) { g_sub[i].h = nullptr; g_sub[i].prefix[0] = 0; any = true; }
  unlockEb();
  return any;
}

uint32_t EventBus::lastSeq() { return g_seq; }

int EventBus::since(uint32_t seq, Event* out, int max, uint32_t* lost) {
  if (lost) *lost = 0;
  if (!out || max <= 0) return 0;
  int n = 0;
  lockEb();
  uint32_t last = g_seq;
  uint32_t oldest = last > CAP ? last - CAP + 1 : 1;       // il piu vecchio ancora presente
  uint32_t from = seq + 1;
  if (seq > last) from = oldest;                           // numero dal futuro (scheda riavviata): riparti da cio che c'e
  if (from < oldest) { if (lost) *lost = oldest - from; from = oldest; }
  for (uint32_t s = from; s <= last && n < max; s++) out[n++] = g_ring[s % CAP];
  unlockEb();
  return n;
}

String EventBus::json(uint32_t seq, int max) {
  if (max > 40) max = 40;
  Event ev[40]; uint32_t lost = 0;
  int n = since(seq, ev, max, &lost);
  String j; j.reserve(60 + n * 70);
  j += "{\"last\":"; j += String((unsigned long)lastSeq());
  j += ",\"lost\":"; j += String((unsigned long)lost);
  j += ",\"ev\":[";
  for (int i = 0; i < n; i++) {
    if (i) j += ',';
    j += "{\"n\":"; j += String((unsigned long)ev[i].seq);
    j += ",\"ms\":"; j += String((unsigned long)ev[i].ms);
    j += ",\"t\":\""; j += ev[i].topic;
    j += "\",\"a\":\""; j += ev[i].arg;
    j += "\",\"v\":"; j += String((long)ev[i].val); j += '}';
  }
  j += "]}";
  return j;
}

uint32_t EventBus::ramBytes() { return sizeof(g_ring) + sizeof(g_sub) + 16; }

void EventBus::reset() { lockEb(); g_seq = 0; memset(g_ring, 0, sizeof(g_ring)); memset(g_sub, 0, sizeof(g_sub)); unlockEb(); }
