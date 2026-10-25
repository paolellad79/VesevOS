// VesevOS Recovery - lettura di /vesevos.conf (vedi rec_config.h). Formato: righe "config <tipo> '<nome>'" e "option <chiave> '<valore>'".
#include "rec_config.h"
#include "rec_util.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <esp_vfs.h>
#include <esp_littlefs.h>

RecCfg rc;

// Valore tra apici con la regola OpenWrt: ' diventa '\''
static std::string parseQuoted(const std::string& l, size_t p) {
  std::string v;
  while (p < l.size() && l[p] != '\'') p++;
  if (p >= l.size()) return v;
  p++;
  while (p < l.size()) {
    if (l[p] == '\'') {
      if (p + 3 < l.size() && l[p + 1] == '\\' && l[p + 2] == '\'' && l[p + 3] == '\'') { v += '\''; p += 4; continue; }
      break;
    }
    v += l[p++];
  }
  return v;
}

static int toInt(const std::string& v) { return atoi(v.c_str()); }

// Una riga "option chiave 'valore'" dentro la sezione di tipo "type" (ui = indice dell'utente corrente).
static void applyOption(const std::string& type, int ui, const std::string& k, const std::string& v) {
  if (type == "system") {
    if (k == "hostname" && v.size()) rc.host = v;
  } else if (type == "region") {
    if (k == "country") rc.country = v;
  } else if (type == "led") {
    if (k == "pin") { int n = toInt(v); if (n >= 0 && n <= 48) rc.ledPin = n; }
    else if (k == "brightness") { int n = toInt(v); if (n >= 0 && n <= 255) rc.ledBr = n < 40 ? 40 : n; }
  } else if (type == "client") {
    if (k == "enabled") rc.staOn = (v == "1");
    else if (k == "ssid") rc.ssid = v;
    else if (k == "pass") rc.pass = v;
    else if (k == "dhcp") rc.dhcp = (v != "0");
    else if (k == "ip") rc.ip = v;
    else if (k == "mask") rc.mask = v;
    else if (k == "gw") rc.gw = v;
    else if (k == "dns1") rc.dns1 = v;
    else if (k == "dns2") rc.dns2 = v;
  } else if (type == "user" && ui >= 0) {
    RecUser& u = rc.users[ui];
    if (k == "name") u.name = v;
    else if (k == "role") u.role = toInt(v);
    else if (k == "on") u.on = (v == "1");
    else if (k == "salt") u.salt = v;
    else if (k == "hash") u.hash = v;
  }
}

static bool readLine(FILE* f, std::string& out) {
  out.clear();
  int c;
  while ((c = fgetc(f)) != EOF) {
    if (c == '\n') return true;
    if (out.size() < 4096) out += (char)c;
  }
  return !out.empty();
}

static void parseFile(FILE* f) {
  std::string line, type;
  int ui = -1;
  while (readLine(f, line)) {
    line = trimStr(line);
    if (line.compare(0, 7, "config ") == 0) {
      size_t sp = line.find(' ', 7);
      type = line.substr(7, sp != std::string::npos && sp > 0 ? sp - 7 : std::string::npos);
      ui = -1;
      if (type == "user" && rc.nUsers < 8) ui = rc.nUsers++;
      continue;
    }
    if (line.compare(0, 7, "option ") != 0) continue;
    size_t sp = line.find(' ', 7);
    if (sp == std::string::npos) continue;
    applyOption(type, ui, line.substr(7, sp - 7), parseQuoted(line, sp));
  }
}

void loadConfig() {
  esp_vfs_littlefs_conf_t c = {};
  c.base_path = "/littlefs";
  c.partition_label = "spiffs";
  c.format_if_mount_failed = false;                 // mai formattare: sono i file dell'utente
  c.read_only = true;
  esp_err_t e = esp_vfs_littlefs_register(&c);
  if (e != ESP_OK) { printf("[recovery] LittleFS non montato (%s): nessuna configurazione\n", esp_err_to_name(e)); return; }
  FILE* f = fopen("/littlefs/vesevos.conf", "r");
  if (!f) {
    printf("[recovery] /vesevos.conf non trovato: nessuna configurazione\n");
    esp_vfs_littlefs_unregister("spiffs");
    return;
  }
  parseFile(f);
  fclose(f);
  esp_vfs_littlefs_unregister("spiffs");
  rc.read = true;
  for (int i = 0; i < rc.nUsers; i++) {
    const RecUser& u = rc.users[i];
    if (u.name.size() && u.on && u.role == 2 && u.hash.size() == 64) rc.adminOk = true;
  }
  printf("[recovery] configurazione letta: rete di casa %s, amministratori %s\n",
         (rc.staOn && rc.ssid.size()) ? "impostata" : "non impostata", rc.adminOk ? "presenti" : "NESSUNO");
}

int userFind(const std::string& n) {
  for (int i = 0; i < rc.nUsers; i++) if (rc.users[i].name.size() && rc.users[i].name == n) return i;
  return -1;
}

std::string recNetName() {
  if (!rc.read) return "vesevos";                    // come la 0.4.4 (il motivo e gia stato scritto da loadConfig)
  std::string h;
  for (char c : rc.host) {
    if (h.size() >= 63) break;
    h += (isalnum((unsigned char)c) || c == '-') ? (char)tolower((unsigned char)c) : '-';
  }
  while (!h.empty() && h.back() == '-') h.pop_back();
  while (!h.empty() && h.front() == '-') h.erase(0, 1);
  return h.empty() ? "vesevos" : h;
}
