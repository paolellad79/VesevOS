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
#include "../net/vos_wps.h"
#include "../packages/vos_mqtt.h"
#include "../devices/vos_led.h"
#include "../devices/vos_pins.h"
#include "../devices/vos_dev.h"
#include "../security/vos_crypt.h"
#include "vos_shell.h"
#include "../core/vos_util.h"
#include "../core/vos_parse.h"
#include "../core/vos_strbuf.h"
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
#include "../packages/vos_selftest.h"
#include "../core/vos_diario.h"
#include "../drivers/vos_drv_fs.h"
#include "../drivers/vos_drv_wifi.h"
#include "../drivers/vos_drv_ota.h"
#include "../core/vos_recupd.h"
#include "../core/vos_updorder.h"
#include "../core/vos_crash.h"
#include <algorithm>
#include <PsychicHttp.h>
#include <PsychicHttpsServer.h>


typedef PsychicRequest Req;
typedef PsychicResponse Res;
enum Lvl : uint8_t { L_PUB = 0, L_GUEST, L_OPER, L_ADMIN };
struct Ctx { int user; uint8_t role; uint32_t ip; bool secure; };
typedef esp_err_t (*Handler)(Req*, Res*, Ctx&);

// Stampa su std::string (per la shell web), al massimo 6000 caratteri
class StrPrint : public OutSink {
 public:
  std::string s;
  size_t write(uint8_t c) override { if (s.size() < 6000) s += (char)c; return 1; }
  size_t write(const uint8_t* b, size_t n) override { for (size_t i = 0; i < n; i++) write(b[i]); return n; }
};

namespace webx {
extern PsychicHttpServer* g_http;
extern PsychicHttpsServer* g_https;
extern bool g_tlsUp;
extern volatile bool g_cleanArm;
extern uint16_t g_runHttpPort, g_runHttpsPort;
extern bool g_httpMini, g_httpMiniWanted;
extern uint32_t g_firstPassUntil;
extern VosFile g_upFile;
extern bool g_upOk;
extern ErrBuf g_upErr;     // motivo del rifiuto del caricamento (vuoto = nessuno)
extern char   g_upPath[160];   // file in caricamento
extern volatile uint32_t g_lastReq;
extern volatile uint32_t g_sleepSec;

uint32_t    ipOf(Req* r);
// Psychic con Arduino da' String/IPAddress, senza Arduino std::string/esp_ip4_addr_t: questi aiuti accettano tutti e due (stesso codice per le due build)
inline const char* textOf(const char* s) { return s; }
template <class T> inline const char* textOf(const T& s) { return s.c_str(); }
// con campo .addr (esp_ip4_addr_t, little endian): si gira in ordine di rete
template <class T> inline auto ipNum(const T& a) -> decltype((void)a.addr, uint32_t()) { uint32_t v = a.addr; return (v << 24) | ((v & 0xFF00u) << 8) | ((v >> 8) & 0xFF00u) | (v >> 24); }
// con indice (IPAddress)
template <class T> inline auto ipNum(const T& a) -> decltype((void)a[0], uint32_t()) { return ((uint32_t)a[0] << 24) | ((uint32_t)a[1] << 16) | ((uint32_t)a[2] << 8) | (uint32_t)a[3]; }
const char* Pc(Req* r, const char* name);                       // el parametro SENZA copiarlo ("" se manca); vale finche vive la richiesta
bool        Pis(Req* r, const char* name, const char* value);   // il parametro e uguale a value?
long        Pint(Req* r, const char* name);                     // numero del parametro (0 se manca o non e un numero)
std::string_view Pt(Req* r, const char* name);                  // il parametro senza spazi ai lati (come trim), SENZA copiarlo; vale finche vive la richiesta
bool        has(Req* r, const char* name);
bool        qparamTo(Req* r, const char* name, char* out, size_t cap);   // parametro dalla riga dell'indirizzo (?nome=valore), decodificato; false se manca (out = "")
esp_err_t   sendText(Res* s, int code, const char* type, const char* body);                // testo chiuso con 0
esp_err_t   sendText(Res* s, int code, const char* type, const char* body, size_t len);   // testo con lunghezza nota
esp_err_t   sendJson(Res* s, const char* body, int code = 200);
esp_err_t   sendJson(Res* s, const HeapBuf& body, int code = 200);
esp_err_t   sendJsonBuf(Res* s, const std::string& body, int code = 200);   // JSON gia in un std::string
esp_err_t   ok(Res* s);
esp_err_t   ko(Res* s, const char* e, const char* code = "error");   // codice HTTP dal "code" (regole API 10-11)
inline esp_err_t ko(Res* s, const ErrBuf& e, const char* code = "error") { return ko(s, e.c_str(), code); }
esp_err_t   notFound(Res* s);
esp_err_t   sendFile(Res* s, const char* fsPath, const char* contentType, bool download);   // un file della memoria interna (vos_web_sendfile.cpp)
void        tokenTo(Req* r, char* out, size_t cap);             // cookie di sessione in out ("" se manca)
int         userOf(Req* r);                                     // utente della sessione di questa richiesta, -1 se non c'e
void        whoTo(const Ctx& c, char* out, size_t n);           // "web <utente> <ip>" per il registro
void        add(PsychicHttpServer* srv, bool secure, const char* path, int method, uint8_t lvl, Handler h);   // registra con livello di accesso
std::string svcJson();
#define SETTINGS_JSON_MAX 2048
bool        settingsJson(HeapBuf& out);                         // false = manca la memoria o supera il tetto
void        delayedRestart(void*);
void        delayedSleep(void*);
esp_err_t   sendPage(Res* s);
#define COOKIE_ATTRS "; Path=/; HttpOnly; SameSite=Strict"   // + "; Secure" sotto HTTPS
esp_err_t   loginDone(Res* s, Ctx& c, int idx);
bool        redirectActive(uint32_t ip);    // vos_web_redirect.cpp: da HTTP a HTTPS per questo indirizzo? (impostazione redirect)
esp_err_t   redirectReply(Req* r, Res* s);     // 307 verso HTTPS (GET e HEAD), errore breve per gli altri metodi
esp_err_t   httpNotFound(Req* r, Res* s);      // percorso sconosciuto sul server HTTP

// Pezzi di JSON dentro un HeapBuf: ,"chiave":numero / true|false / "testo protetto".
inline void jsonNum(HeapBuf& o, const char* key, long v) { char t[40]; snprintf(t, sizeof(t), ",\"%s\":%ld", key, v); o.add(t); }
inline void jsonBool(HeapBuf& o, const char* key, bool v) { o.add(",\""); o.add(key); o.add(v ? "\":true" : "\":false"); }
inline void jsonStr(HeapBuf& o, const char* key, const char* v) { o.add(",\""); o.add(key); o.add("\":\""); o.addJson(v, strlen(v)); o.add('"'); }
}  // namespace webx
using namespace webx;

// gruppi di percorsi (uno per file); routes() in vos_web.cpp li chiama tutti
void routesAuth(PsychicHttpServer* S, bool sec);    // vos_web_auth.cpp : accesso, MFA, utenti
void routesDev(PsychicHttpServer* S, bool sec);     // vos_web_dev.cpp  : periferiche, pin, LED, automazioni
void routesSd(PsychicHttpServer* S, bool sec);      // vos_web_sd.cpp   : scheda microSD (senza String)
void routesSys(PsychicHttpServer* S, bool sec);     // vos_web_sys.cpp  : MQTT, autodiagnosi, ora, localizzazione
void routesNet(PsychicHttpServer* S, bool sec);     // vos_web_net.cpp  : configurazione, filtro IP, watchdog, servizi di rete, HTTPS
void routesFiles(PsychicHttpServer* S, bool sec);   // vos_web_files.cpp: rete tra schede, Bluetooth, file
void routesScript(PsychicHttpServer* S, bool sec);  // vos_web_script.cpp: pagina Script (elenco, stato, avvio, stop)
void routesApp(PsychicHttpServer* S, bool sec);     // vos_web_app.cpp: pagina App (elenco e controllo dei file .vap)
void routesUpd(PsychicHttpServer* S, bool sec);     // vos_web_upd.cpp: aggiornamento tramite recovery (ordine in NVS, esito)
