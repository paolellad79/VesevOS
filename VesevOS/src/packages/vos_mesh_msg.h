// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_mesh_msg.h
// Parti della rete tra schede che non toccano la radio: lettura dei messaggi, JSON e testo di stato. Niente Arduino, niente String:
// lavorano su std::string_view (viste, non copiano) e su StrBuf (buffer fisso). Si provano sul computer (tools/hosttest/test_mesh.cpp).
// Regola dei JSON: o entrano interi nel buffer o si rifiutano (StrBuf::truncated()), mai JSON a meta.
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <string_view>
#include "../core/vos_strbuf.h"

// ---- limiti (i campi sono a lunghezza fissa: niente memoria dinamica per ogni nodo o messaggio) ----
constexpr size_t MESH_MAXPAY     = 180;   // byte utili di un messaggio
constexpr size_t MESH_NAME_CAP   = 33;    // nome di una scheda (32 + zero)
constexpr size_t MESH_STATE_CAP  = 96;    // riga di stato di un nodo (la nostra e lunga circa 90)
constexpr size_t MESH_FROM_CAP   = 72;    // "mittente -> destinatario"
constexpr size_t MESH_WHEN_CAP   = 32;
constexpr size_t MESH_FIELDS     = 7;     // nome|ip|segnale|acceso da|temperatura|cpu|versione

struct MeshNode {
  uint8_t  mac[6];
  char     name[MESH_NAME_CAP];
  uint8_t  role, hops;
  int8_t   rssi;
  uint32_t last;
  uint16_t boot;
  uint32_t seq;
  char     state[MESH_STATE_CAP];
  bool     ccx, used;
};
struct MeshMsg { char from[MESH_FROM_CAP]; char text[MESH_MAXPAY + 1]; char when[MESH_WHEN_CAP]; };

// Copia una vista in un buffer fisso: tronca se non ci sta e chiude sempre con zero.
inline void meshCopy(char* dst, size_t cap, std::string_view v) {
  if (!cap) return;
  size_t n = v.size() < cap - 1 ? v.size() : cap - 1;
  if (n) memcpy(dst, v.data(), n);
  dst[n] = 0;
}

// Azioni ammesse da un'altra scheda: solo la prima parola (fino al primo spazio, che non sia il primo carattere) conta.
inline bool meshCmdAllowed(std::string_view c) {
  size_t sp = c.find(' ');
  std::string_view w = (sp != std::string_view::npos && sp > 0) ? c.substr(0, sp) : c;
  return w == "led" || w == "led-color" || w == "led-bright" || w == "gpio" || w == "note" || w == "reboot" || w == "ntp";
}

inline void meshMacStr(const uint8_t* m, char out[18]) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

// Chiave comune: 64 cifre esadecimali -> 32 byte. Come prima, non controlla le cifre (una coppia non valida vale 0): conta la lunghezza.
inline bool meshKeyParse(std::string_view hex, uint8_t out[32]) {
  if (hex.size() != 64) return false;
  for (int i = 0; i < 32; i++) { char p[3] = { hex[i * 2], hex[i * 2 + 1], 0 }; out[i] = (uint8_t)strtoul(p, nullptr, 16); }
  return true;
}

// Beacon "canale|nome": il nome e quello che segue la prima barra (o tutto il testo se non c'e).
inline std::string_view meshBeaconName(std::string_view pay) {
  size_t p = pay.find('|');
  return p != std::string_view::npos ? pay.substr(p + 1) : pay;
}
// Stato "nome|ip|...": il nome e quello che precede la prima barra (se la barra e il primo carattere, vale tutto il testo).
inline std::string_view meshStateName(std::string_view pay) {
  size_t p = pay.find('|');
  return (p != std::string_view::npos && p > 0) ? pay.substr(0, p) : pay;
}

// Divide lo stato nei suoi 7 campi (i mancanti restano vuoti; il testo oltre il settimo campo si ignora).
inline void meshSplitState(std::string_view pay, std::string_view f[MESH_FIELDS]) {
  for (size_t i = 0; i < MESH_FIELDS; i++) f[i] = std::string_view();
  size_t k = 0, s = 0;
  for (size_t i = 0; i <= pay.size() && k < MESH_FIELDS; i++)
    if (i == pay.size() || pay[i] == '|') { f[k++] = pay.substr(s, i - s); s = i + 1; }
}

// Testo di un messaggio: toglie "\r", "a capo" diventa spazio, fuori da 32..126 (tab escluso) diventa '?'.
// Scrive in out al massimo keep caratteri; ritorna la lunghezza che avrebbe tutto il testo pulito.
inline size_t meshCleanText(const char* in, std::string& out, size_t keep) {
  size_t n = 0;
  for (const unsigned char* p = (const unsigned char*)(in ? in : ""); *p; p++) {
    char c;
    if (*p == '\r') continue;
    else if (*p == '\n') c = ' ';
    else if (*p == '\t' || (*p >= 32 && *p < 127)) c = (char)*p;
    else c = '?';
    if (n < keep) out.push_back(c);
    n++;
  }
  return n;
}

// ---- JSON ----
inline void meshJsonStr(StrBuf& sb, std::string_view v) {   // stesso testo che dava jsonEscape()
  for (unsigned char c : v) {
    if (c == '"') sb.add("\\\"");
    else if (c == '\\') sb.add("\\\\");
    else if (c == '\n') sb.add("\\n");
    else if (c == '\r') sb.add("\\r");
    else if (c == '\t') sb.add("\\t");
    else if (c < 32 || c >= 127) sb.add('?');
    else sb.add((char)c);
  }
}
// Un numero intero con segno (al massimo 11 caratteri): vuoto o altro = "0". Cosi un nodo non puo infilare altro JSON nei campi numerici.
inline void meshJsonNum(StrBuf& sb, std::string_view v) {
  size_t i = (!v.empty() && v[0] == '-') ? 1 : 0;
  bool ok = v.size() > i && v.size() <= 11;
  for (size_t k = i; ok && k < v.size(); k++) if (v[k] < '0' || v[k] > '9') ok = false;
  if (ok) sb.addn(v.data(), v.size()); else sb.add('0');
}

// Messaggio MQTT con lo stato di un nodo (solo il gateway lo pubblica).
inline void meshStateJson(StrBuf& sb, const std::string_view f[MESH_FIELDS], int hops) {
  sb.add("{\"ip\":\""); meshJsonStr(sb, f[1]);
  sb.add("\",\"rssi\":"); meshJsonNum(sb, f[2]);
  sb.add(",\"uptime\":"); meshJsonNum(sb, f[3]);
  sb.add(",\"temp\":"); meshJsonNum(sb, f[4]);
  sb.add(",\"cpu\":"); meshJsonNum(sb, f[5]);
  sb.add(",\"ver\":\""); meshJsonStr(sb, f[6]);
  sb.addf("\",\"hops\":%d}", hops);
}

// Dati per il JSON di /api/mesh (stato del servizio, nodi, ultimi messaggi).
struct MeshJsonIn {
  bool run, autoOn, hasKey; int role, ch, chMax; const char* me;
  uint32_t sent, recv, bad, nowMs;
  const MeshNode* nodes; int nodeN;    // nodeN = dimensione dell'array, si saltano quelli non usati
  const MeshMsg* msgs; int msgN;       // dal piu vecchio al piu nuovo; nel JSON escono dal piu nuovo
};

inline void meshJsonBuild(StrBuf& sb, const MeshJsonIn& in) {
  sb.addf("{\"run\":%s,\"auto\":%s,\"role\":%d,\"hasKey\":%s,\"ch\":%d,\"chMax\":%d,\"me\":\"", in.run ? "true" : "false", in.autoOn ? "true" : "false",
          in.role, in.hasKey ? "true" : "false", in.ch, in.chMax);
  sb.add(in.me ? in.me : "");
  sb.addf("\",\"sent\":%lu,\"recv\":%lu,\"bad\":%lu,\"nodes\":[", (unsigned long)in.sent, (unsigned long)in.recv, (unsigned long)in.bad);
  bool first = true;
  for (int i = 0; i < in.nodeN; i++) {
    const MeshNode& n = in.nodes[i];
    if (!n.used) continue;
    if (!first) sb.add(',');
    first = false;
    char mac[18]; meshMacStr(n.mac, mac);
    sb.addf("{\"mac\":\"%s\",\"name\":\"", mac); meshJsonStr(sb, n.name);
    sb.addf("\",\"role\":%d,\"hops\":%d,\"rssi\":%d,\"ago\":%lu,\"ccx\":%s,\"state\":\"", (int)n.role, (int)n.hops, (int)n.rssi,
            (unsigned long)((in.nowMs - n.last) / 1000), n.ccx ? "true" : "false");
    meshJsonStr(sb, n.state); sb.add("\"}");
  }
  sb.add("],\"msgs\":[");
  for (int i = in.msgN - 1; i >= 0; i--) {
    sb.add("{\"from\":\""); meshJsonStr(sb, in.msgs[i].from);
    sb.add("\",\"text\":\""); meshJsonStr(sb, in.msgs[i].text);
    sb.add("\",\"when\":\""); meshJsonStr(sb, in.msgs[i].when);
    sb.add("\"}");
    if (i) sb.add(',');
  }
  sb.add("]}");
}

// Costruisce un testo di misura esatta: una prima passata conta i caratteri (StrBuf senza buffer), la seconda scrive in un solo blocco
// della misura giusta (piu un po' di margine, perche i dati possono cambiare tra le due passate). Se non entra neanche al secondo giro
// ritorna vuoto: il chiamante risponde con un errore, mai con un testo (o un JSON) troncato. fill(StrBuf&) puo prendere il suo blocco.
template <class Fill> inline std::string meshBuild(Fill fill) {
  for (int giro = 0; giro < 2; giro++) {
    StrBuf conta(nullptr, 0);
    fill(conta);
    StrText t(conta.required() + 1 + (giro ? 2048 : 128));
    fill(t.buf());
    if (!t.truncated()) return t.take();
  }
  return std::string();
}
