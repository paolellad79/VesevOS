// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_wire.cpp
#include "vos_wire.h"
#include "core/vos_hooks.h"
#include "core/vos_cpupm.h"
#include "core/vos_sys.h"
#include "core/vos_wd.h"
#include "drivers/vos_drv_heap.h"
#include "interfaces/vos_web.h"
#include "interfaces/vos_shell.h"
#include "packages/vos_rules.h"
#include "packages/vos_mqtt.h"
#include "packages/vos_mesh.h"
#include "packages/vos_ble.h"
#include "packages/vos_mfa.h"
#include "packages/vos_btn.h"
#include "packages/vos_dev_pkg.h"
#include "packages/vos_rec.h"
#include "devices/vos_led.h"
#include "devices/vos_dev.h"
#include "devices/vos_io.h"
#include "net/vos_net.h"
#include "net/vos_traffic.h"

static bool mqttPub(const char* sub, const char* msg) { return mqttPublishRel(sub, msg, false); }
static void radiosOff() { meshStop(); bleStop(); }
static bool heapIntegraW() { return drvHeapIntegra(); }
static void riavviaPulitoW(const char* motivo) { sysReboot(motivo, 0); }

static void shellExecW(const char* line, OutSink& out, int role) { shellExec(line, out, role); }
static bool devIsOnW(const char* id) { const DevOps* d = devFind(id); return d && d->state && d->state(); }
static NetState netStateW() { return netState(); }
static int cpuTickW(float tempC, int busyPct) { cpuPmTick(tempC, busyPct); return (int)cpuPmThermal(); }

void vosWire() {
  g_hooks.webInit      = webInit;
  g_hooks.webStart     = webStart;
  g_hooks.webSospendi  = webSospendi;
  g_hooks.webRiprendi  = webRiprendi;
  g_hooks.taskSospendiTutti = sysTaskSospendiTutti;
  g_hooks.taskRiprendiUno = sysTaskRiprendiUno;
  g_hooks.heapIntegra  = heapIntegraW;
  g_hooks.riavviaPulito = riavviaPulitoW;
  g_hooks.webRotte     = webRotte;
  g_hooks.webHttpsUp   = webHttpsUp;
  g_hooks.webTrimIdle  = webTrimIdle;
  g_hooks.rulesInit    = rulesInit;
  g_hooks.mqttInit     = mqttInit;
  g_hooks.meshInit     = meshInit;
  g_hooks.mqttPublish  = mqttPub;
  g_hooks.mfaOn        = mfaOn;
  g_hooks.btnWpsClicks = btnWpsClicks;
  g_hooks.radiosOff    = radiosOff;
  g_hooks.ledInit      = ledInit;
  g_hooks.netInit      = netInit;
  g_hooks.ledAlarm     = ledSetAlarm;
  g_hooks.ledFault     = ledSetFault;
  g_hooks.ledShutdown  = ledShutdown;
  g_hooks.netState     = netStateW;
  g_hooks.trafficTick  = trafficTick;
  g_hooks.ioAllOff     = ioAllOff;
  g_hooks.devIsOn      = devIsOnW;
  g_hooks.devOff       = devOffId;
  g_hooks.shellExec    = shellExecW;
  g_hooks.webIdleSec   = webIdleSec;
  g_hooks.cpuTick      = cpuTickW;
  g_hooks.cpuApplyMode = cpuPmApplyMode;
  g_hooks.cpuStatus    = cpuPmStatusJson;
#if VOS_WITH_REC
  g_hooks.recFlush     = recFlushNow;
#endif
  devPkgRegister();
}
