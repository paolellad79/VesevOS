// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_berry_svc.cpp
// STRATO 6 (packages): il servizio "vdb" nel registro dei servizi (vos_service). E l'INTERRUTTORE del SOTTOSISTEMA Berry (script, app e comandi dei
// driver): acceso di fabbrica; spento ferma il programma in corso, svuota la coda e impedisce nuovi avvii (vedi startRun in vos_berry.cpp).
// Lo stato si salva in /vesevos.conf (sezione vdb). Lo accende e lo spegne solo l'Admin, dai Task o con "vesevos-services vdb start|stop".
#include "vos_berry.h"

#if VOS_WITH_BERRY
#include "vos_berry_int.h"
#include "../core/vos_config.h"
#include "../core/vos_service.h"
#include "../core/vos_audit.h"
#include "../core/vos_i18n.h"

static void imposta(bool on) {
  cfg.berryOn = on;
  cfgSetOrigin("vdb");
  cfgSave();
  auditClear("vdb");
  auditEvent(AUD_INFO, "vdb", on ? tr("Sottosistema vdb acceso") : tr("Sottosistema vdb spento"));
}

static int svcState() { return cfg.berryOn ? SVC_ON : SVC_OFF; }

static bool svcBegin(ErrBuf&) {
  imposta(true);
  return true;
}

static void svcStop() {
  qClear();                                  // niente script in coda: partirebbero appena si riaccende
  imposta(false);                            // prima il divieto, poi si ferma: nessun nuovo avvio puo infilarsi
  if (berryRunning()) { ErrBuf e; berryStop(e); }
}

static bool svcJson(HeapBuf& out) {
  BerryRunInfo b;
  bool run = berryRunInfo(b);
  return out.addf("{\"have\":true,\"on\":%s,\"run\":%d}", cfg.berryOn ? "true" : "false", run && !b.cmd ? 1 : 0);
}

static const ServiceOps BERRY_OPS = { "vdb", "Sottosistema vdb", "Sistema", "task", ROLE_OPER, ROLE_ADMIN,
                                      svcState, svcBegin, svcStop, nullptr, svcJson, nullptr, nullptr };

void berryServiceInit() { serviceRegister(&BERRY_OPS); }
#endif
