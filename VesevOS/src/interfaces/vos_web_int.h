// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_int.h
// INTERNO al server web: tipi, aiuti e dati condivisi tra vos_web.cpp e i file dei percorsi (vos_web_auth/dev/sys/net/files.cpp).
// Non includerlo da altri moduli: l'interfaccia pubblica e vos_web.h.
#pragma once
#include "vos_web.h"
#include "../core/vos_i18n.h"
#include "../core/vos_license.h"
#include "vos_page.h"
#include "../core/vos_config.h"
#include "../security/vos_auth.h"
#include "../core/vos_sys.h"
#include "../net/vos_net.h"
#include "../packages/vos_mqtt.h"
#include "../devices/vos_led.h"
#include "../devices/vos_pins.h"
#include "../devices/vos_dev.h"
#include "../security/vos_crypt.h"
#include "vos_shell.h"
#include "../core/vos_util.h"
#include "../core/vos_files.h"
#include "../core/vos_time.h"
#include "../packages/vos_rules.h"
#include "../core/vos_boot.h"
#include "../core/vos_region.h"
#include "../core/vos_audit.h"
#include "../core/vos_fw.h"
#include "../core/vos_wd.h"
#include "../security/vos_tls.h"
#include "../packages/vos_mesh.h"
#include "../packages/vos_ble.h"
#include "../core/vos_service.h"
#include "../core/vos_serial.h"
#include "../packages/vos_mfa.h"
#include "../packages/vos_power.h"
#include "../packages/vos_stats.h"
#include "../core/vos_log.h"
#include "../core/vos_selftest.h"
#include "../core/vos_diario.h"
#include "../drivers/vos_drv_fs.h"
#include "../drivers/vos_drv_wifi.h"
#include "../drivers/vos_drv_ota.h"
#include <PsychicHttp.h>
#include <PsychicHttpsServer.h>


typedef PsychicRequest Req;
typedef PsychicResponse Res;
enum Lvl : uint8_t { L_PUB = 0, L_GUEST, L_OPER, L_ADMIN };
struct Ctx { int user; uint8_t role; uint32_t ip; bool secure; };
typedef esp_err_t (*Handler)(Req*, Res*, Ctx&);

// Stampa su String (per la shell web)
class StrPrint : public Print {
 public:
  String s;
  size_t write(uint8_t c) override { if (s.length() < 6000) s += (char)c; return 1; }
  size_t write(const uint8_t* b, size_t n) override { for (size_t i = 0; i < n; i++) write(b[i]); return n; }
};

namespace webx {
extern PsychicHttpServer* g_http;
extern PsychicHttpsServer* g_https;
extern bool g_tlsUp;
extern volatile bool g_cleanArm;
extern uint16_t g_runHttpPort, g_runHttpsPort;
extern uint32_t g_firstPassUntil;
extern File g_upFile;
extern bool g_upOk;
extern String g_upErr, g_upPath;
extern volatile uint32_t g_lastReq;
extern volatile uint32_t g_sleepSec;

uint32_t    ipOf(Req* r);
String      P(Req* r, const char* name);
bool        has(Req* r, const char* name);
String      qparam(Req* r, const char* name);
esp_err_t   sendText(Res* s, int code, const char* type, const String& body);
esp_err_t   sendJson(Res* s, const String& body, int code = 200);
esp_err_t   ok(Res* s);
esp_err_t   ko(Res* s, const String& e, const char* code = "error");   // codice HTTP dal "code" (regole API 10-11)
esp_err_t   notFound(Res* s);
String      token(Req* r);
String      who(const Ctx& c);
void        add(PsychicHttpServer* srv, bool secure, const char* path, int method, uint8_t lvl, Handler h);   // registra con livello di accesso
String      svcJson();
String      settingsJson();
void        delayedRestart(void*);
void        delayedSleep(void*);
esp_err_t   sendPage(Res* s);
String      cookieAttrs(bool secure);
esp_err_t   loginDone(Res* s, Ctx& c, int idx);
}  // namespace webx
using namespace webx;

// gruppi di percorsi (uno per file); routes() in vos_web.cpp li chiama tutti
void routesAuth(PsychicHttpServer* S, bool sec);    // vos_web_auth.cpp : accesso, MFA, utenti
void routesDev(PsychicHttpServer* S, bool sec);     // vos_web_dev.cpp  : periferiche, pin, LED, automazioni
void routesSys(PsychicHttpServer* S, bool sec);     // vos_web_sys.cpp  : MQTT, autodiagnosi, ora, localizzazione
void routesNet(PsychicHttpServer* S, bool sec);     // vos_web_net.cpp  : configurazione, filtro IP, watchdog, servizi di rete, HTTPS
void routesFiles(PsychicHttpServer* S, bool sec);   // vos_web_files.cpp: rete tra schede, Bluetooth, file
