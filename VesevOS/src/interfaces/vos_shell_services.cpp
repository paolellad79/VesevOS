// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_services.cpp
// Comando "vesevos-services": elenco dei servizi con stato, porta e RAM, e start/stop/restart/status di uno solo (stile systemctl semplice).
//
//   vesevos-services                       elenco (tutti i ruoli)
//   vesevos-services <servizio> [status]   stato di uno (con i dettagli dove ci sono)
//   vesevos-services <servizio> start|stop|restart   solo Admin
//
// Nessun servizio nuovo e nessuna logica nuova: le azioni passano dalle stesse funzioni che usano gia i comandi mqtt, mesh, ble, ntp e svc
// (e la pagina). I comandi svc, start, restart, kill, mqtt, mesh, ble, ntp restano com'erano. Il ruolo del comando e Operatore (tabella dei ruoli):
// chi non e Admin puo solo leggere, e qui si controlla.
//
// Servizi di SOLA LETTURA (oggi non si fermano da shell): wifi (si spegne con il modo aereo), wps (comando wps), rules, script, wd.
// http, https e ap: la scelta si salva ma vale dal riavvio (come "svc"); fermarli puo tagliare fuori chi sta usando la scheda, quindi chiedono
// conferma (seriale y/n, pagina: " yes" in fondo alla riga).
#include "vos_shell_int.h"
#include "vos_shell_ask.h"

namespace {

enum Sid : uint8_t { S_HTTP, S_HTTPS, S_REDIRECT, S_AP, S_CAPTIVE, S_DHCP, S_MDNS, S_WIFI, S_WPS, S_MQTT, S_MESH, S_BLE, S_NTP, S_RULES, S_SCRIPT, S_WD };

struct Row {
  const char* name;      // come si scrive nel comando (nomi propri e sigle: non si traducono)
  Sid         id;
  bool        ctl;       // si puo avviare e fermare da qui?
  bool        risky;     // fermarlo puo tagliare fuori l'utente: chiede conferma
};

const Row kRows[] = {
  {"http", S_HTTP, true, true},   {"https", S_HTTPS, true, true},  {"redirect", S_REDIRECT, true, false},
  {"ap", S_AP, true, true},       {"captive", S_CAPTIVE, true, false}, {"dhcp", S_DHCP, true, false},
  {"mdns", S_MDNS, true, false},  {"wifi", S_WIFI, false, false},  {"wps", S_WPS, false, false},
  {"mqtt", S_MQTT, true, false},  {"mesh", S_MESH, true, false},   {"ble", S_BLE, true, false},
  {"ntp", S_NTP, true, false},    {"rules", S_RULES, false, false}, {"script", S_SCRIPT, false, false},
  {"wd", S_WD, false, false},
};

// Il servizio del registro (vos_service.h) che sta dietro alla riga, o nullptr se la riga non ne ha uno.
const char* registryId(Sid s) {
  switch (s) {
    case S_WIFI: return "wifi";
    case S_WPS:  return "wps";
    case S_MQTT: return "mqtt";
    case S_MESH: return "mesh";
    case S_BLE:  return "ble";
    default:     return nullptr;
  }
}

bool present(Sid s) {
  const char* id = registryId(s);
  return !id || serviceFind(id) != nullptr;                // MQTT, mesh, BLE e WPS possono non essere nel firmware
}

int stateOf(Sid s) {
  if (const char* id = registryId(s)) return serviceState(id);
  switch (s) {
    case S_HTTP:     return cfg.httpOn ? SVC_ON : SVC_OFF;
    case S_HTTPS:    return !cfg.https ? SVC_OFF : webHttpsUp() ? SVC_ON : SVC_STARTING;
    case S_REDIRECT: return cfg.httpRedirect ? SVC_ON : SVC_OFF;
    case S_AP:       return cfg.apOn ? SVC_ON : SVC_OFF;
    case S_CAPTIVE:  return netCaptive() ? SVC_ON : SVC_OFF;
    case S_DHCP:     return cfg.dhcpOn ? SVC_ON : SVC_OFF;
    case S_MDNS:     return cfg.mdnsOn ? SVC_ON : SVC_OFF;
    case S_NTP:      return cfg.ntpOn ? SVC_ON : SVC_OFF;
    case S_RULES:    return sysTaskRunning("rules") ? SVC_ON : SVC_OFF;
    case S_SCRIPT:   return berryRunning() ? SVC_ON : SVC_OFF;
    case S_WD:       return sysTaskRunning("wd") ? SVC_ON : SVC_OFF;
    default:         return SVC_OFF;
  }
}

const char* stateWord(int st) {
  switch (st) {
    case SVC_ON:         return tr("attivo");
    case SVC_ON_PROBLEM: return tr("con problemi");
    case SVC_STARTING:   return tr("in avvio");
    case SVC_ERROR:      return tr("errore");
    default:             return tr("fermo");
  }
}

unsigned portOf(Sid s) {
  switch (s) {
    case S_HTTP:  return cfg.httpPort;
    case S_HTTPS: return cfg.httpsPort;
    case S_MQTT:  return cfg.mqttPort;
    case S_MDNS:  return 5353;
    default:      return 0;
  }
}

const Row* findRow(const char* name) {
  for (const Row& r : kRows) if (!strcmp(r.name, name) && present(r.id)) return &r;
  return nullptr;
}

// Una riga di tabella: nome, stato, porta e RAM (stimata) dove si conoscono, "-" altrimenti.
void printRow(OutSink& o, const Row& r) {
  char port[8] = "-", ram[12] = "-";
  if (portOf(r.id)) snprintf(port, sizeof port, "%u", portOf(r.id));
  if (const char* id = registryId(r.id)) {
    uint32_t b = serviceRam(id);
    if (b) snprintf(ram, sizeof ram, "%u KB", (unsigned)((b + 512) / 1024));
  }
  o.printf("%-10s %-13s %-6s %s\r\n", r.name, stateWord(stateOf(r.id)), port, ram);
}

void svcList(OutSink& o) {
  o.println(tr("Servizio   Stato         Porta  RAM"));
  for (const Row& r : kRows) if (present(r.id)) printRow(o, r);
  o.println(tr("Cambiare (solo Admin): vesevos-services <servizio> start|stop|restart. Di sola lettura: wifi, wps, rules, script, wd."));
  o.println(tr("http, https e ap: la modifica vale dal riavvio. Task e altri servizi: ps, start, kill."));
}

void svcStatus(OutSink& o, const Row& r) {
  o.println(trf("%s: %s", r.name, stateWord(stateOf(r.id))));
  if (r.id == S_MQTT) o.print(mqttStatusText().c_str());
  else if (r.id == S_MESH) o.print(meshText().c_str());
  else if (r.id == S_BLE) o.print(bleText().c_str());
  else if (r.id == S_HTTPS && cfg.https && !webHttpsUp()) o.println(tr("HTTPS acceso nella configurazione ma non ancora in funzione (serve un riavvio o il certificato e in preparazione)"));
}

// Esegue la riga di un comando che gia esiste (svc, ntp): stessi controlli e stessi messaggi di quando la scrive l'utente.
void runOld(OutSink& o, int role, const char* oldLine) {
  Word c = argAt(oldLine, 0), a1 = argAt(oldLine, 1);
  if (!shCmdSys(c, a1, oldLine, o, role)) shCmdNet(c, a1, oldLine, o, role);
}

// Avvia, ferma o riavvia. act: 1 start, 2 stop, 3 restart.
void svcAct(OutSink& o, int role, const Row& r, int what) {
  ErrBuf err;
  char old[48];
  switch (r.id) {
    case S_MQTT: case S_MESH: {
      bool ok = what == 1 ? serviceStart(r.name, err) : what == 3 ? serviceRestart(r.name, err) : serviceStop(r.name, err);
      if (!ok) { o.println(err.c_str()); return; }
      break;
    }
    case S_BLE: {
      bool ok = true;
      if (what == 1) ok = bleUserOn(0, err);                    // come "ble on": senza limite di tempo
      else if (what == 2) bleUserOff();
      else ok = serviceRestart("ble", err);
      if (!ok) { o.println(err.c_str()); return; }
      break;
    }
    case S_NTP:
      if (what == 3) runOld(o, role, "ntp sync");               // riavviare il client NTP = risincronizzare
      else runOld(o, role, what == 1 ? "ntp on" : "ntp off");
      return;
    default:                                                    // http, https, redirect, ap, captive, dhcp, mdns: come "svc <nome> on|off"
      if (what == 3) { o.println(tr("Questo servizio non si riavvia da qui: usa stop e poi start")); return; }
      snprintf(old, sizeof old, "svc %s %s", r.name, what == 1 ? "on" : "off");
      runOld(o, role, old);
      return;
  }
  o.println(trf("%s: %s", r.name, stateWord(stateOf(r.id))));
}

void servicesCmd(const Word& a1, const char* line, OutSink& o, int role) {
  if (a1.empty()) { svcList(o); return; }
  Word name = a1;
  name.lower();
  const Row* r = findRow(name.c_str());
  if (!r) { o.println(trf("Servizio sconosciuto: %s (elenco: vesevos-services)", name.c_str())); return; }
  Word w = argAt(line, 2);
  w.lower();
  if (w.empty() || w == "status") { svcStatus(o, *r); return; }
  int what = w == "start" ? 1 : w == "stop" ? 2 : w == "restart" ? 3 : 0;
  if (!what) { o.println(tr("Uso: vesevos-services [<servizio> start|stop|restart|status]")); return; }
  if (role != ROLE_ADMIN) { o.println(tr("Serve il livello Admin")); return; }
  if (!r->ctl) { o.println(trf("%s e di sola lettura: non si ferma da qui", r->name)); return; }
  if (r->risky && what != 1 && !askConfirmed(line)) {
    askConfirm(o, line, trf("Fermare %s puo tagliarti fuori dalla scheda (vale dal riavvio). Procedo?", r->name));
    return;
  }
  svcAct(o, role, *r, what);
}

}  // namespace

bool shServicesActive(HeapBuf& out) {
  bool ok = true;
  for (const Row& r : kRows) {
    if (!present(r.id) || stateOf(r.id) == SVC_OFF) continue;
    if (out.length()) ok = ok && out.add(", ");
    ok = ok && out.add(r.name);
  }
  return ok;
}

bool shCmdVesevos(const Word& c, const Word& a1, const char* line, OutSink& o, int role, uint32_t sess) {
  if (c == "vesevos-services") servicesCmd(a1, line, o, role);
  else if (c == "vesevos-bugreport") shBugreport(line, o, role);
  else if (c == "vesevos-config") shConfig(line, o, role, sess);
  else if (c == "vesevos-network") shNetwork(line, o, role, sess);
  else if (c == "vesevos-cleaner") shCleaner(line, o, role);
  else if (c == "vesevos-drive_manager") shDriveManager(line, o, role, sess);
  else if (c == "vesevos-backup") shBackup(line, o, role);
  else if (c == "vesevos-update") shUpdate(line, o, role);
  else return false;
  return true;
}
