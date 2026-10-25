// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_mqtt_msg.h
// Parti del client MQTT che non toccano ne la rete ne Arduino: prefisso degli argomenti, confronto dell'argomento di un comando in arrivo,
// pulizia del testo di un comando, JSON dello stato e della discovery di Home Assistant. Niente String: lavorano su std::string_view
// (viste, non copiano) e su StrBuf (buffer fisso). Si provano sul computer (tools/hosttest/test_mqtt.cpp).
// Regola dei JSON: o entrano interi nel buffer o si scartano (StrBuf::truncated()), mai JSON a meta.
#pragma once
#include <stdint.h>
#include <string.h>
#include <string_view>
#include "../core/vos_parse.h"
#include "../core/vos_strbuf.h"

// ---- prefisso degli argomenti: due pezzi, cosi non serve una copia ----
// Se l'utente ne ha scelto uno si usa quello (senza spazi ai lati); altrimenti "vesevos/" + nome della scheda. Le barre finali si tolgono.
struct MqttPrefix {
  std::string_view a, b;
  size_t size() const { return a.size() + b.size(); }
};
inline MqttPrefix mqttPrefixParts(std::string_view cfgPrefix, std::string_view hostname) {
  MqttPrefix p;
  std::string_view u = trimView(cfgPrefix);
  if (!u.empty()) { p.a = u; }
  else { p.a = std::string_view("vesevos/", 8); p.b = hostname; }
  for (;;) {
    std::string_view& last = !p.b.empty() ? p.b : p.a;
    if (last.empty() || last.back() != '/') break;
    last.remove_suffix(1);
  }
  return p;
}
// Scrive il prefisso in out (con zero finale). Ritorna false se non ci sta tutto (out resta chiuso con zero).
inline bool mqttPrefixCopy(const MqttPrefix& p, char* out, size_t cap) {
  if (!cap) return false;
  StrBuf s(out, cap); s.addn(p.a.data(), p.a.size()); s.addn(p.b.data(), p.b.size());
  return !s.truncated();
}
// ---- comando in arrivo: solo caratteri stampabili (gli altri diventano spazio), chiuso con zero. out deve avere len + 1 byte ----
inline void mqttCmdClean(const char* data, int len, char* out) {
  for (int i = 0; i < len; i++) { char c = data[i]; out[i] = (c >= 32 && c < 127) ? c : ' '; }
  out[len < 0 ? 0 : len] = 0;
}

// ---- identificativo della scheda: "vesevos_" + MAC senza i due punti, in minuscolo ----
inline void mqttDevId(std::string_view mac, StrBuf& o) {
  o.add("vesevos_");
  for (char c : mac) { if (c == ':') continue; o.add((char)((c >= 'A' && c <= 'Z') ? c + 32 : c)); }
}

// ---- testo dentro virgolette JSON: stesse regole di jsonEscape() ----
inline void mqttJsonEsc(StrBuf& o, std::string_view s) {
  for (char ch : s) {
    unsigned char c = (unsigned char)ch;
    if (c == '"') o.add("\\\"");
    else if (c == '\\') o.add("\\\\");
    else if (c == '\n') o.add("\\n");
    else if (c == '\r') o.add("\\r");
    else if (c == '\t') o.add("\\t");
    else if (c < 32 || c >= 127) o.add('?');
    else o.add((char)c);
  }
}

// ---- JSON dello stato (ogni N secondi su <prefisso>/state) ----
inline void mqttStateJson(StrBuf& o, int cpu, int temp, uint32_t heap, uint32_t psram, int rssi, uint32_t uptime,
                          const char* ip, const char* mac, const char* version) {
  o.addf("{\"cpu\":%d,\"temp\":%d,\"heap\":%lu,\"psram\":%lu,\"rssi\":%d,\"uptime\":%lu", cpu, temp, (unsigned long)heap, (unsigned long)psram, rssi, (unsigned long)uptime);
  o.add(",\"ip\":\""); o.add(ip); o.add("\"");
  o.add(",\"mac\":\""); o.add(mac); o.add("\"");
  o.add(",\"version\":\""); o.add(version); o.add("\"}");
}

// ---- Home Assistant: la scheda si presenta da sola, un sensore per ogni valore ----
inline void mqttHaDev(StrBuf& o, const char* id, std::string_view hostname, const char* board, const char* version) {
  o.add("\"dev\":{\"ids\":[\""); o.add(id); o.add("\"],\"name\":\""); mqttJsonEsc(o, hostname);
  o.add("\",\"mdl\":\""); o.add(board); o.add("\",\"mf\":\"VesevOS\",\"sw\":\""); o.add(version); o.add("\"}");
}
inline void mqttHaAv(StrBuf& o, const MqttPrefix& p) {
  o.add("\"avty_t\":\""); mqttJsonEsc(o, p.a); mqttJsonEsc(o, p.b); o.add("/status\"");
}
inline void mqttHaSensor(StrBuf& o, const char* id, const char* key, const char* name, const char* unit, const char* cls,
                         const MqttPrefix& p, const char* av, const char* dev) {
  o.add("{\"name\":\""); o.add(name); o.add("\",\"uniq_id\":\""); o.add(id); o.add("_"); o.add(key);
  o.add("\",\"stat_t\":\""); mqttJsonEsc(o, p.a); mqttJsonEsc(o, p.b); o.add("/state\",\"val_tpl\":\"{{ value_json."); o.add(key); o.add(" }}\"");
  if (unit[0]) { o.add(",\"unit_of_meas\":\""); o.add(unit); o.add("\""); }
  if (cls[0]) { o.add(",\"dev_cla\":\""); o.add(cls); o.add("\""); }
  o.add(",\"stat_cla\":\"measurement\","); o.add(av); o.add(","); o.add(dev); o.add("}");
}
inline void mqttHaRebootButton(StrBuf& o, const char* id, const MqttPrefix& p, const char* av, const char* dev) {
  o.add("{\"name\":\"Riavvia\",\"uniq_id\":\""); o.add(id); o.add("_reboot\",\"cmd_t\":\""); mqttJsonEsc(o, p.a); mqttJsonEsc(o, p.b);
  o.add("/cmd\",\"pl_prs\":\"reboot\",\"dev_cla\":\"restart\","); o.add(av); o.add(","); o.add(dev); o.add("}");
}

// Azzera un buffer che conteneva un segreto (utente, password): il compilatore non puo togliere la scrittura (puntatore volatile).
inline void mqttSecureZero(void* p, size_t n) {
  volatile unsigned char* v = (volatile unsigned char*)p;
  while (n--) *v++ = 0;
}
