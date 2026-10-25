// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_dev.cpp
// Percorsi del server web: periferiche, pin, LED, automazioni. Aiuti e tipi condivisi: vos_web_int.h.
#include "vos_web_int.h"
#include "vos_shell_sess.h"
#include "vos_shell_prompt.h"
#include "vos_shell_complete.h"
#include "../core/vos_eventbus.h"
#include "../devices/vos_io.h"
#include "../devices/vos_mod.h"
#include "../packages/vos_btn.h"
#include "../core/vos_clamp.h"

// Copia in out (cap compreso lo zero) il testo in minuscolo; chi chiama ha gia controllato che ci stia
static void lowerCopy(std::string_view v, char* out, size_t cap) {
  size_t n = v.size() < cap - 1 ? v.size() : cap - 1;
  for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)v[i]);
  out[n] = 0;
}

// Risposta JSON da un buffer riempito da una funzione "...Json(HeapBuf&)": se la funzione e fallita, errore di memoria
static esp_err_t sendBuf(Res* s, bool filled, const HeapBuf& j) {
  if (!filled) return ko(s, tr("Memoria insufficiente"), "off");
  return sendText(s, 200, "application/json", j.c_str(), j.length());
}

// "web <utente> <ip>": chi ha fatto la richiesta, per il registro (vive fino alla fine dell'espressione)
struct WhoText {
  char s[96];
  explicit WhoText(const Ctx& c) { whoTo(c, s, sizeof s); }
};

// Evento nel registro di sicurezza con testo formattato (il formato arriva gia tradotto da tr)
static void auditf(AuditLevel lv, const char* key, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
static void auditf(AuditLevel lv, const char* key, const char* fmt, ...) {
  char t[200];
  va_list ap; va_start(ap, fmt);
  vsnprintf(t, sizeof t, fmt, ap);
  va_end(ap);
  auditEvent(lv, key, t);
}

// Tab nel terminale della pagina: stesso completamento della seriale (vos_shell_complete.h) con il ruolo di chi chiede.
// Risposta {"ok":true,"add":"testo da aggiungere","list":["nome",...],"more":false}. Il testo del comando non e mai eseguito ne registrato.
static esp_err_t shellTab(Res* s, int role, uint32_t sess, const char* line) {
  ShellCompletion* sc = (ShellCompletion*)malloc(sizeof(ShellCompletion));
  if (!sc) return ko(s, tr("Memoria insufficiente"));
  size_t room = strlen(line) < 200 ? 200 - strlen(line) : 0;           // la riga non supera i 200 caratteri, come per i comandi
  shellComplete(sess, role, line, room, true, *sc);
  HeapBuf j(4096);
  j.add("{\"ok\":true,\"add\":\""); j.addJson(sc->add, strlen(sc->add)); j.add("\",\"list\":[");
  for (int i = 0; i < sc->n; i++) {
    if (i) j.add(',');
    j.add('"'); j.addJson(sc->name(i), strlen(sc->name(i))); j.add('"');
  }
  j.add("],\"more\":"); j.add(sc->more ? "true" : "false"); j.add('}');
  free(sc);
  return sendBuf(s, !j.oom() && !j.over(), j);
}

void routesDev(PsychicHttpServer* S, bool sec) {
  // ---- periferiche (virtuali e hardware): stesso modo di rispondere per tutte ----
  add(S, sec, "/api/dev", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf j(DEV_JSON_MAX);
    if (has(r, "id")) {
      bool found = devStatusJson(Pc(r, "id"), c.role, j);
      if (!found && !j.oom() && !j.over()) return notFound(s);        // sconosciuta o ruolo insufficiente
      return sendBuf(s, found, j);
    }
    return sendBuf(s, devListJson(j), j);
  });
  add(S, sec, "/api/events", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    long n = has(r, "n") ? Pint(r, "n") : 20;
    std::string j = EventBus::json((uint32_t)std::max(0L, Pint(r, "since")), (int)vosClamp(n, 1L, 40L));
    return j.empty() ? ko(s, tr("Memoria esaurita"), "off") : sendJsonBuf(s, j);
  });
  add(S, sec, "/api/dev/set", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    ErrBuf e; return devSet(Pc(r, "id"), Pis(r, "on", "1"), e) ? ok(s) : ko(s, e);
  });
  add(S, sec, "/api/dev/act", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf out(512); ErrBuf e;
    if (!devAct(Pc(r, "id"), Pc(r, "a"), Pc(r, "arg"), c.role, out, e)) return ko(s, e);
    return out.length() ? sendText(s, 200, "application/json", out.c_str(), out.length()) : ok(s);
  });
  // Periferiche a classi (rele, led, pin, ingresso, analogico, i2c): elenco per chi puo vedere, modifiche solo Admin
  add(S, sec, "/api/io", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf j(IO_JSON_MAX);                                         // el JSON si scrive qui (heap) e si invia senza copiarlo in una String
    if (!ioListJson(j)) return ko(s, tr("Memoria insufficiente"));
    return sendText(s, 200, "application/json", j.c_str(), j.length());
  });
  add(S, sec, "/api/io/add", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    char nm[16]; ErrBuf e;
    if (!ioAdd(Pc(r, "cls"), (int)Pint(r, "gpio"), Pc(r, "name"), Pc(r, "opt"), nm, e)) return ko(s, e);
    auditf(AUD_INFO, "", tr("periferica %s creata (%s)"), nm, WhoText(c).s);
    char body[40];
    snprintf(body, sizeof body, "{\"ok\":true,\"n\":\"%s\"}", nm);
    return sendText(s, 200, "application/json", body, strlen(body));
  });
  add(S, sec, "/api/io/del", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    ErrBuf e; if (!ioRemove(Pc(r, "n"), e)) return ko(s, e);
    auditf(AUD_INFO, "", tr("periferica %s tolta (%s)"), Pc(r, "n"), WhoText(c).s); return ok(s);
  });
  add(S, sec, "/api/io/rename", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { ErrBuf e; return ioRename(Pc(r, "n"), Pc(r, "to"), e) ? ok(s) : ko(s, e); });
  // Schede modulo (.vdp): elenco e dettagli per chi puo vedere; installare, caricare ed eliminare solo Admin
  add(S, sec, "/api/mods", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf j(MOD_LIST_JSON_MAX);                                   // il JSON si scrive qui (heap) e si invia senza copiarlo in una String
    if (!modListJson(j)) return ko(s, tr("Memoria insufficiente"));
    return sendText(s, 200, "application/json", j.c_str(), j.length());
  });
  add(S, sec, "/api/mod", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf j(MOD_INFO_MAX);
    if (!modInfoJson(Pc(r, "name"), j)) return ko(s, tr("Memoria insufficiente"));
    return sendText(s, 200, "application/json", j.c_str(), j.length());
  });
  add(S, sec, "/api/mod/install", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf out(2048); ErrBuf e;
    if (!modInstall(Pc(r, "name"), Pc(r, "prefix"), out, e)) return ko(s, e);
    auditf(AUD_INFO, "", tr("scheda modulo %s installata (%s)"), Pc(r, "name"), WhoText(c).s);
    return sendText(s, 200, "application/json", out.c_str(), out.length());
  });
  add(S, sec, "/api/mod/save", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { ErrBuf e; return modSave(Pc(r, "name"), Pc(r, "text"), e) ? ok(s) : ko(s, e); });
  add(S, sec, "/api/mod/del", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { ErrBuf e; return modDelete(Pc(r, "name"), e) ? ok(s) : ko(s, e); });
  add(S, sec, "/api/pins", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t { HeapBuf j(PIN_JSON_MAX); return sendBuf(s, pinsJson(j), j); });
  add(S, sec, "/api/pins/info", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t { HeapBuf j(PIN_JSON_MAX); return sendBuf(s, pinBoardJson(j), j); });
  add(S, sec, "/api/pinmap", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t { HeapBuf j(PIN_JSON_MAX); return sendBuf(s, pinMapJson(j), j); });
  add(S, sec, "/api/pinnotes", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t { HeapBuf j(PIN_JSON_MAX); return sendBuf(s, pinNotesJson(j), j); });
  add(S, sec, "/api/pinnotes", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { ErrBuf e; return pinNoteSet((int)Pint(r, "g"), Pc(r, "name"), e) ? ok(s) : ko(s, e); });
  add(S, sec, "/api/pintest", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t { HeapBuf j(PIN_JSON_MAX); return sendBuf(s, pinTestJson(j), j); });
  add(S, sec, "/api/pintest", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    ErrBuf err;
    return pinTestRequest((int)Pint(r, "gpio"), Pc(r, "action"), Pc(r, "pull"), err) ? ok(s) : ko(s, err);
  });
  add(S, sec, "/api/log", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    std::string g = logGet(150);
    return sendText(s, 200, "text/plain; charset=utf-8", g.c_str(), g.size());
  });
  add(S, sec, "/api/log/clear", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    logClear(); auditf(AUD_INFO, "", tr("registro svuotato (%s)"), WhoText(c).s); return ok(s);
  });
  add(S, sec, "/api/log/level", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t { char b[24]; snprintf(b, sizeof b, "{\"lv\":%d}", logLevel()); return sendText(s, 200, "application/json", b, strlen(b)); });
  add(S, sec, "/api/log/level", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    int lv = vosClamp((int)Pint(r, "lv"), 0, 3);
    cfg.logLevel = (uint8_t)lv; logSetLevel(lv); cfgSave();
    auditf(AUD_INFO, "", tr("livello del registro %d (%s)"), lv, WhoText(c).s);
    return ok(s);
  });
  add(S, sec, "/api/boots", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { std::string d = diaryText(20, true); return sendText(s, 200, "text/plain; charset=utf-8", d.c_str(), d.size()); });
  add(S, sec, "/api/boots/clear", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    diaryClear(); auditf(AUD_INFO, "", tr("diario dei riavvii azzerato (%s)"), WhoText(c).s); return ok(s);
  });
  add(S, sec, "/api/tasks", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf t(SYS_TASKS_TEXT_MAX);
    if (!sysTasksText(t)) return ko(s, tr("Memoria insufficiente"), "off");
    return sendText(s, 200, "text/plain; charset=utf-8", t.c_str(), t.length());
  });

  add(S, sec, "/api/serial", HTTP_GET, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { HeapBuf j(SERIAL_JSON_MAX); return serialJson(j) ? sendJson(s, j) : ko(s, tr("Memoria insufficiente")); });
  add(S, sec, "/api/serial", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    const char* k = Pc(r, "k"); const char* v = Pc(r, "v"); ErrBuf e;
    if (!strcmp(k, "keep")) { serialKeep(); HeapBuf j(SERIAL_JSON_MAX); return serialJson(j) ? sendJson(s, j) : ko(s, tr("Memoria insufficiente")); }
    if (!serialSet(k, v, e)) return ko(s, e);
    auditf(AUD_INFO, "", tr("seriale: %s = %s (%s)"), k, v, WhoText(c).s);
    HeapBuf j(SERIAL_JSON_MAX);
    return serialJson(j) ? sendJson(s, j) : ko(s, tr("Memoria insufficiente"));
  });
  add(S, sec, "/api/serialauth", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    cfg.serialAuth = !Pis(r, "on", "0"); cfgSave();
    vlog("SICUREZZA: password sulla seriale %s", cfg.serialAuth ? "attivata" : "disattivata");
    return ok(s);
  });
  // ---- tasto BOOT: azione di 1, 2 e 3 click ----
  add(S, sec, "/api/btn", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { HeapBuf j(BTN_JSON_MAX); return sendBuf(s, btnJson(j), j); });
  add(S, sec, "/api/btn", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    ErrBuf e; bool any = false;
    for (int n = 1; n <= BTN_CLICKS; n++) {
      char k[4];
      snprintf(k, sizeof k, "c%d", n);
      if (!has(r, k)) continue;
      if (!btnSetAction(n, (int)Pint(r, k), e)) return ko(s, e);
      any = true;
    }
    if (has(r, "test")) btnTestSet(Pis(r, "test", "1"));
    if (any) { cfgSetOriginf("web admin %s", IpText(c.ip).s); cfgSave(); }
    HeapBuf j(BTN_JSON_MAX);
    return sendBuf(s, btnJson(j), j);
  });
  add(S, sec, "/api/identify", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { ledIdentify(10000); return ok(s); });
  add(S, sec, "/api/tasklist", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf j(SYS_TASKS_JSON_MAX);
    if (!sysTasksJson(j)) return sendJson(s, "{\"ok\":false}");
    return sendText(s, 200, "application/json", j.c_str(), j.length());
  });
  add(S, sec, "/api/taskkill", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { ErrBuf err; return sysTaskKill(Pc(r, "name"), err.data(), err.size()) ? ok(s) : ko(s, err); });
  add(S, sec, "/api/taskrestart", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { ErrBuf err; return sysTaskRestart(Pc(r, "name"), err.data(), err.size()) ? ok(s) : ko(s, err); });

  // Automazioni
  add(S, sec, "/api/rules", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    std::string st = rulesStatusJson();
    HeapBuf text(RULES_READ_MAX), j(2 * RULES_READ_MAX + st.size() + 32);
    if (!rulesText(text)) return ko(s, tr("Memoria insufficiente"), "off");
    j.add("{\"text\":\""); j.addJson(text.c_str(), text.length()); j.add("\",\"st\":"); j.add(st.c_str(), st.size()); j.add('}');
    if (j.oom() || j.over()) return ko(s, tr("Memoria insufficiente"), "off");
    return sendText(s, 200, "application/json", j.c_str(), j.length());
  });
  add(S, sec, "/api/rules", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { ErrBuf e; return rulesSave(Pc(r, "text"), e) ? ok(s) : ko(s, e); });
  add(S, sec, "/api/rules/run", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { ErrBuf e; return rulesRunNow((int)Pint(r, "i"), e) ? ok(s) : ko(s, e); });
  // Ordine di avvio
  add(S, sec, "/api/boot", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { HeapBuf j(BOOT_JSON_MAX); return bootJson(j) ? sendJson(s, j) : ko(s, tr("Memoria insufficiente")); });
  add(S, sec, "/api/boot", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    char res[96]; ErrBuf e;
    if (!bootSetOrder(Pc(r, "order"), res, sizeof res, e)) return ko(s, e);
    HeapBuf j(256);
    j.add("{\"ok\":true,\"order\":\""); j.addJson(res); j.add("\"}");
    return sendBuf(s, !j.oom() && !j.over(), j);
  });
  add(S, sec, "/api/boot/reset", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { bootResetOrder(); return ok(s); });
  add(S, sec, "/api/wifi/scan", HTTP_GET, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    HeapBuf j(NET_SCAN_JSON_MAX);
    if (!netScanJson(j)) return ko(s, tr("Memoria insufficiente"), "off");
    return sendText(s, 200, "application/json", j.c_str(), j.length());
  });
  add(S, sec, "/api/wifi/scan", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t { netScanStart(); return ok(s); });

  add(S, sec, "/api/wifi/test", HTTP_GET, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { 
    HeapBuf j(256);
    netTestJson(j);
    return sendJson(s, j); });
  add(S, sec, "/api/wifi/test", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    const char* ssid = Pc(r, "ssid"); const char* pass = Pc(r, "pass");
    size_t sl = strlen(ssid), pl = strlen(pass);
    if (sl == 0 || sl > 32) return ko(s, tr("Nome rete non valido"));
    if (pl > 0 && (pl < 8 || pl > 63)) return ko(s, tr("Password Wi-Fi: da 8 a 63 caratteri"));
    netTestStart(ssid, pass); return ok(s);
  });
  add(S, sec, "/api/wifi/save", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    const char* ssid = Pc(r, "ssid"); const char* pass = Pc(r, "pass");
    size_t sl = strlen(ssid), pl = strlen(pass);
    bool dhcp = !Pis(r, "dhcp", "0");
    if (sl == 0 || sl > 32) return ko(s, tr("Nome rete non valido"));
    if (pl > 0 && (pl < 8 || pl > 63)) return ko(s, tr("Password Wi-Fi: da 8 a 63 caratteri"));
    if (!dhcp) {
      uint32_t a, m, g, d;
      if (!ipParse(Pc(r, "ip"), a)) return ko(s, tr("IP non valido"));
      if (!ipParse(Pc(r, "mask"), m) || !maskValid(m)) return ko(s, tr("Subnet mask non valida"));
      if (!ipParse(Pc(r, "gw"), g)) return ko(s, tr("Gateway non valido"));
      if (((a ^ g) & m) != 0) return ko(s, tr("Il gateway non e nella stessa rete dell'IP"));
      if (*Pc(r, "d1") && !ipParse(Pc(r, "d1"), d)) return ko(s, tr("DNS 1 non valido"));
      if (*Pc(r, "d2") && !ipParse(Pc(r, "d2"), d)) return ko(s, tr("DNS 2 non valido"));
      cfg.ip = Pc(r, "ip"); cfg.mask = Pc(r, "mask"); cfg.gw = Pc(r, "gw");
      cfg.dns1 = Pc(r, "d1"); cfg.dns2 = Pc(r, "d2");
    }
    g_cleanArm = Pis(r, "cleanup", "1");
    cfg.staSsid = ssid;
    if (pl) cfg.staPass = pass;
    cfg.staDhcp = dhcp; cfg.staEnabled = true;
    bool ntpBack = cfgNtpAfterWifi();
    cfgSave(); netReconfigure(); if (ntpBack) timeApply(); return ok(s);
  });
  add(S, sec, "/api/wifi/ap", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t { cfg.staEnabled = false; cfgSave(); netReconfigure(); return ok(s); });

  // Punto di accesso (hotspot): nome e password. Dopo l'accesso li vede chiunque (scelta del proprietario:
  // il QR stampato da fiducia a chi lo usa); l'amministratore puo nasconderli agli altri (interruttore "apQr").
  add(S, sec, "/api/ap", HTTP_GET, L_GUEST, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    bool show = cfg.apQr || c.role >= ROLE_ADMIN;
    HeapBuf j(256);                                                     // nome (32) e password (63), anche con tutti i caratteri da proteggere
    j.add("{\"ssid\":\""); j.addJson(cfg.apSsid.c_str());
    j.add("\",\"pass\":\""); if (show) j.addJson(cfg.apPass.c_str());
    j.addf("\",\"qr\":%s}", cfg.apQr ? "true" : "false");
    return sendBuf(s, !j.oom() && !j.over(), j);
  });
  add(S, sec, "/api/ap", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (has(r, "qr") && !has(r, "ssid")) { cfg.apQr = Pis(r, "qr", "1"); cfgSave(); return ok(s); }
    std::string_view ss = Pt(r, "ssid");
    FixedStr<16> generated;
    if (Pis(r, "new", "1")) generated = cfgNewApPass();
    const char* pw = generated.empty() ? Pc(r, "pass") : generated.c_str();
    size_t pl = strlen(pw);
    if (ss.empty() || ss.size() > 32) return ko(s, tr("Nome rete non valido"));
    if (pl && (pl < 8 || pl > 63)) return ko(s, tr("Password Wi-Fi: da 8 a 63 caratteri"));
    if (pl && cfgApPassWeak(pw)) return ko(s, tr("Password troppo debole o di fabbrica: le leggi sulla sicurezza (UE RED/EN 18031, CRA; UK PSTI) vietano le password uguali per tutti"));
    cfg.apSsid = ss; if (pl) cfg.apPass = pw;
    cfgSave(); netReconfigure();
    HeapBuf j(160);
    j.add("{\"ok\":true,\"pass\":\""); j.addJson(cfg.apPass.c_str()); j.add("\"}");
    return sendJson(s, j);
  });

  add(S, sec, "/api/shell", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    const char* cmd = Pc(r, "c");
    if (strlen(cmd) > 200) return sendText(s, 200, "text/plain; charset=utf-8", tr("Comando troppo lungo"));
    uint32_t sess = SESS_WEB + (uint32_t)(c.user > 0 ? c.user : 0);   // la cartella corrente e per utente
    if (Pint(r, "tab") == 1) return shellTab(s, c.role, sess, cmd);    // Tab nel terminale: non esegue niente, risponde coi candidati
    StrPrint sp;
    cfgSetOriginf("shell %s", WhoText(c).s);
    // La password di passwd / user add / wifi set arriva nel campo "pw", mai dentro il comando "c": cosi la pagina non la mette in cronologia.
    // Il comando risponde nell'intestazione X-VOS-Ask se vuole una conferma ("confirm") o una password ("secret", "secret2": da ripetere).
    char askKind[10];
    shellExecWeb(cmd, has(r, "pw") ? Pc(r, "pw") : nullptr, sp, c.role, sess, askKind, sizeof askKind, c.ip);
    if (askKind[0]) s->addHeader("X-VOS-Ask", askKind);
    char prompt[SHELL_PROMPT_MAX];                                    // il nuovo prompt (cartella e hostname di adesso) va nell'intestazione
    shellPrompt(sess, c.user >= 0 && c.user < VOS_MAX_USERS ? cfg.users[c.user].name.c_str() : "", c.role, prompt, sizeof prompt);
    s->addHeader("X-VOS-Prompt", prompt);
    size_t cap = sp.s.size() + 1;
    char* clean = (char*)malloc(cap);
    if (!clean) return ko(s, tr("Memoria insufficiente"));
    size_t n = cleanAscii(sp.s, clean, cap);
    esp_err_t e = sendText(s, 200, "text/plain; charset=utf-8", clean, n);
    free(clean);
    return e;
  });

  add(S, sec, "/api/led", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    int m = (int)Pint(r, "mode"), b = (int)Pint(r, "br");
    if (m < 0 || m >= LED_MODE_COUNT || b < 0 || b > 255) return ko(s, tr("Valori non validi"));
    // trigger flash, wifi, cpu, colori per sorgente e soglia (campi s0..s4 e thr): solo Admin, come il comando "led trigger"
    bool hasSrc = Pc(r, "thr")[0] != 0;
    for (int i = 0; i < LEDSRC_COUNT; i++) {
      char key[4] = {'s', (char)('0' + i), 0};
      if (Pc(r, key)[0]) hasSrc = true;
    }
    bool newTrigger = m >= LED_FLASH && m != cfg.ledMode;
    if ((hasSrc || newTrigger) && c.role < ROLE_ADMIN) return ko(s, tr("Serve il livello Admin"));
    long thr = Pc(r, "thr")[0] ? Pint(r, "thr") : cfg.ledCpuThr;
    if (thr < 10 || thr > 95) return ko(s, tr("Valori non validi"));
    for (int i = 0; i < LEDSRC_COUNT; i++) {
      char key[4] = {'s', (char)('0' + i), 0};
      if (Pc(r, key)[0]) cfg.ledSrcColor[i] = (uint32_t)strtoul(Pc(r, key), NULL, 10) & 0xFFFFFF;
    }
    cfg.ledCpuThr = (uint8_t)thr;
    cfg.ledMode = m; cfg.ledBrightness = b;
    cfg.ledColor = (uint32_t)strtoul(Pc(r, "color"), NULL, 10) & 0xFFFFFF;
    ledApplyConfig(); cfgSave(); return ok(s);
  });

  add(S, sec, "/api/system", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    std::string_view hv = Pt(r, "hostname"), dv = Pt(r, "domain");
    if (!hostnameValid(hv)) return ko(s, tr("Nome host non valido (1-32 caratteri: lettere, numeri, trattino; non all'inizio o alla fine)"));
    if (!domainValid(dv)) return ko(s, tr("Dominio non valido (nomi separati da punti, solo lettere, numeri e trattino)"));
    char hn[40], dm[64];                                                  // gia controllati: hostname fino a 32, dominio fino a 60
    lowerCopy(hv, hn, sizeof hn); lowerCopy(dv, dm, sizeof dm);
    bool changed = (cfg.hostname != hn);
    cfg.hostname = hn; cfg.domain = dm;
    cfgSave();
    if (changed) netReconfigure();
    return ok(s);
  });

  add(S, sec, "/api/config/download", HTTP_GET, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    s->addHeader("Content-Disposition", "attachment; filename=\"vesevos.conf\"");
    HeapBuf t(CFG_TEXT_MAX);
    if (!cfgExport(t, false)) return ko(s, tr("Memoria insufficiente"));
    return sendText(s, 200, "text/plain; charset=utf-8", t.c_str(), t.length());
  });
  // esportazione con i segreti: solo Admin, solo dopo aver accettato (acc=1), sempre cifrata con una frase (mai in chiaro)
  add(S, sec, "/api/config/export", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (!Pis(r, "acc", "1")) return ko(s, tr("Devi accettare l'avviso per esportare i segreti"));
    HeapBuf plainCfg(CFG_TEXT_MAX); ErrBuf err;
    if (!cfgExport(plainCfg, true)) return ko(s, tr("Memoria insufficiente"));
    HeapBuf out(CRYPT_OUT_MAX(plainCfg.length()));
    if (!cryptSeal(plainCfg.c_str(), Pc(r, "pw"), out, err)) return ko(s, err);
    vlog("CFG: esportazione con segreti (cifrata) da %s", WhoText(c).s);
    auditf(AUD_YELLOW, "cfgexp", tr("Backup con segreti scaricato da %s"), WhoText(c).s);
    s->addHeader("Content-Disposition", "attachment; filename=\"vesevos-segreti.conf\"");
    return sendText(s, 200, "text/plain; charset=utf-8", out.c_str(), out.length());
  });
  add(S, sec, "/api/config/restore", HTTP_POST, L_ADMIN, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    const char* t = Pc(r, "t"); ErrBuf err;
    size_t tl = strlen(t);
    if (tl == 0 || tl > 46000) return ko(s, tr("File vuoto o troppo grande"), "toobig");
    HeapBuf plain(tl);
    if (cryptIsSealed(t)) {
      if (!*Pc(r, "pw")) return ko(s, tr("Il file e cifrato: scrivi la frase"));
      if (!cryptOpen(t, Pc(r, "pw"), plain, err)) { authFailIp(c.ip, "frase del backup sbagliata"); return ko(s, err); }
      t = plain.c_str(); tl = plain.length();
    }
    HeapBuf before(CFG_TEXT_MAX), after(CFG_TEXT_MAX);
    if (!cfgExport(before, true)) return ko(s, tr("Memoria insufficiente"));
    if (!cfgImport(t, tl, err)) return ko(s, err);
    cfgSave(); ledApplyConfig(); netReconfigure();
    if (!cfgExport(after, true)) return ko(s, tr("Memoria insufficiente"));
    int ch = 0;                                                          // righe cambiate rispetto a prima
    std::string_view was(before.c_str(), before.length()), now(after.c_str(), after.length());
    for (size_t i = 0; i < now.size(); ) {
      size_t e = now.find('\n', i);
      size_t end = e == std::string_view::npos ? now.size() : e + 1;     // la riga con il suo a-capo
      if (end - i > 1 && was.find(now.substr(i, end - i)) == std::string_view::npos) ch++;
      i = end;
    }
    vlog("CFG: ripristino da file (%d righe cambiate) da %s", ch, WhoText(c).s);
    char b[40];
    snprintf(b, sizeof b, "{\"ok\":true,\"changed\":%d}", ch);
    return sendJson(s, b);
  });

  add(S, sec, "/api/airplane", HTTP_POST, L_OPER, [](Req* r, Res* s, Ctx& c) -> esp_err_t {
    if (!Pis(r, "on", "1")) { netAirplaneOff("dalla pagina"); return ok(s); }
    ErrBuf err;
    return netAirplaneOn((int)Pint(r, "exit"), strtoul(Pc(r, "param"), NULL, 10), err) ? ok(s) : ko(s, err);
  });
}
