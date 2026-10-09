// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_service.cpp
// Registro dei servizi: nessuna dipendenza dall'hardware (si prova sul computer).
#include "vos_service.h"
#include "vos_eventbus.h"

static const ServiceOps* g_svc[VOS_MAX_SERVICES];
static int g_n = 0;

bool serviceRegister(const ServiceOps* s) {
  if (!s || !s->id || !s->state || !s->begin || !s->stop) return false;
  for (int i = 0; i < g_n; i++) if (String(g_svc[i]->id) == s->id) return false;
  if (g_n >= VOS_MAX_SERVICES) return false;
  g_svc[g_n++] = s;
  return true;
}
int serviceCount() { return g_n; }
const ServiceOps* serviceAt(int i) { return (i >= 0 && i < g_n) ? g_svc[i] : nullptr; }
const ServiceOps* serviceFind(const String& id) {
  for (int i = 0; i < g_n; i++) if (id == g_svc[i]->id) return g_svc[i];
  return nullptr;
}
int serviceState(const String& id) { const ServiceOps* s = serviceFind(id); return s ? s->state() : SVC_OFF; }
bool serviceStart(const String& id, String& err) {
  const ServiceOps* s = serviceFind(id);
  if (!s) { err = "servizio sconosciuto"; return false; }
  bool r = s->begin(err);
  EventBus::publish("svc.state", s->id, s->state());
  return r;
}
bool serviceStop(const String& id, String& err) {
  const ServiceOps* s = serviceFind(id);
  if (!s) { err = "servizio sconosciuto"; return false; }
  s->stop();
  EventBus::publish("svc.state", s->id, s->state());
  return true;
}
bool serviceRestart(const String& id, String& err) {
  const ServiceOps* s = serviceFind(id);
  if (!s) { err = "servizio sconosciuto"; return false; }
  bool r;
  if (s->restart) r = s->restart(err);
  else { s->stop(); r = s->begin(err); }
  EventBus::publish("svc.state", s->id, s->state());
  return r;
}
uint32_t serviceRam(const String& id) { const ServiceOps* s = serviceFind(id); return (s && s->ramBytes) ? s->ramBytes() : 0; }
