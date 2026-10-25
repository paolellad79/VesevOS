// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_util.cpp
#include "vos_util.h"
#include "../drivers/vos_drv_sys.h"
#include "vos_i18n.h"
#include "vos_parse.h"
#include "mbedtls/sha256.h"
#include "esp_system.h"

bool ipParse(std::string_view in, uint32_t& out) {
  std::string_view t = trimView(in);
  if (t.size() < 7 || t.size() > 15) return false;
  uint32_t parts[4]; int n = 0; size_t start = 0;
  for (size_t i = 0; i <= t.size(); i++) {
    if (i < t.size() && t[i] != '.') continue;
    if (n >= 4 || i == start) return false;
    uint32_t v = 0;
    for (size_t k = start; k < i; k++) {
      if (!isdigit((unsigned char)t[k])) return false;
      v = v * 10 + (uint32_t)(t[k] - '0');
      if (v > 255) return false;
    }
    parts[n++] = v; start = i + 1;
  }
  if (n != 4) return false;
  out = (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
  return true;
}

void ipTo(uint32_t ip, char* out, size_t n) {
  snprintf(out, n, "%u.%u.%u.%u", (unsigned)((ip >> 24) & 255), (unsigned)((ip >> 16) & 255), (unsigned)((ip >> 8) & 255), (unsigned)(ip & 255));
}

bool maskValid(uint32_t m) {
  if (m == 0) return false;
  uint32_t inv = ~m;
  if ((inv & (inv + 1)) != 0) return false;     // deve essere 1111..0000
  int ones = 0;
  for (int i = 0; i < 32; i++) if (m & (1UL << i)) ones++;
  return ones >= 8 && ones <= 30;
}

void uptimeTo(uint64_t s, char* b, size_t n) {
  if (!b || !n) return;
  // 1 anno = 360 giorni, 1 mese = 30 giorni
  uint64_t y = s / (360ULL * 86400); s %= (360ULL * 86400);
  uint64_t mo = s / (30ULL * 86400); s %= (30ULL * 86400);
  uint64_t d = s / 86400; s %= 86400;
  uint64_t h = s / 3600; s %= 3600;
  uint64_t mi = s / 60; s %= 60;
  // si mostrano solo le unita da quella piu grande non zero in poi
  size_t len = 0;
  b[0] = 0;
  auto put = [&](uint64_t v, const char* unit, const char* sep) {
    if (len >= n) return;
    int w = snprintf(b + len, n - len, "%lu%s%s", (unsigned long)v, unit, sep);
    if (w > 0) len += ((size_t)w < n - len) ? (size_t)w : n - len - 1;
  };
  bool started = false;
  if (y)               { put(y, tr("a"), " "); started = true; }
  if (started || mo)   { put(mo, tr("m"), " "); started = true; }
  if (started || d)    { put(d, tr("g"), " "); started = true; }
  if (started || h)    { put(h, tr("h"), " "); started = true; }
  if (started || mi)   { put(mi, tr("min"), " "); }
  put(s, tr("s"), "");
}

void durStr(uint64_t s, char* b, size_t n) {
  if (!b || !n) return;
  unsigned long d = (unsigned long)(s / 86400ULL), h = (unsigned long)(s % 86400ULL / 3600ULL), m = (unsigned long)(s % 3600ULL / 60ULL);
  if (s < 60)         snprintf(b, n, "%lu %s", (unsigned long)s, tr("s"));
  else if (s < 3600)  snprintf(b, n, "%lu %s", m, tr("min"));
  else if (s < 86400) snprintf(b, n, "%lu %s %lu %s", h, tr("h"), m, tr("min"));
  else                snprintf(b, n, "%lu %s %lu %s %lu %s", d, tr("g"), h, tr("h"), m, tr("min"));
}

size_t cleanAscii(std::string_view in, char* out, size_t cap) {
  if (!out || !cap) return 0;
  size_t w = 0;
  for (char ch : in) {
    unsigned char c = (unsigned char)ch;
    if (c == '\r') continue;                      // a-capo di Windows: si toglie
    if (w + 1 >= cap) break;
    out[w++] = (c == '\n' || c == '\t' || (c >= 32 && c < 127)) ? (char)c : '?';
  }
  out[w] = 0;
  return w;
}

void sha256HexOf(std::string_view data, char out[65]) {
  unsigned char h[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(&ctx, (const unsigned char*)data.data(), data.size());
  mbedtls_sha256_finish(&ctx, h);
  mbedtls_sha256_free(&ctx);
  static const char* hx = "0123456789abcdef";
  for (int i = 0; i < 32; i++) { out[i * 2] = hx[h[i] >> 4]; out[i * 2 + 1] = hx[h[i] & 15]; }
  out[64] = 0;
}

bool utilNameOk(const char* s, size_t n) {
  if (n < 1 || n > 24) return false;
  for (size_t i = 0; i < n; i++) { char c = s[i]; if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false; }
  return true;
}

bool utilLineErr(ErrBuf& err, int line, const char* msg) { err.setf(tr("Riga %d: "), line); err.append(msg); return false; }

static bool labelValid(std::string_view l) {
  if (l.size() < 1 || l.size() > 32) return false;
  if (l.front() == '-' || l.back() == '-') return false;
  for (char c : l) if (!(isalnum((unsigned char)c) || c == '-')) return false;
  return true;
}

bool hostnameValid(std::string_view s) { return labelValid(s); }

bool domainValid(std::string_view s) {
  if (s.empty()) return true;
  if (s.size() > 60) return false;
  size_t start = 0;
  for (size_t i = 0; i <= s.size(); i++) {
    if (i == s.size() || s[i] == '.') {
      if (!labelValid(s.substr(start, i - start))) return false;
      start = i + 1;
    }
  }
  return true;
}

// F1 (1.7.9a): un argomento e una parola, oppure un testo tra virgolette doppie ("Casa mia").
// Dentro le virgolette \" vale una virgoletta. Se manca la virgoletta finale si prende tutto fino alla fine.
bool utilTakeArg(std::string_view s, int& pos, char* out, size_t cap) {
  size_t w = 0;
  auto put = [&](char c) { if (w + 1 < cap) out[w++] = c; };      // oltre la capacita si scarta
  if (cap) out[0] = 0;
  int n = (int)s.size();
  while (pos < n && s[pos] == ' ') pos++;
  if (pos >= n) return false;
  if (s[pos] == '"') {
    pos++;
    while (pos < n && s[pos] != '"') {
      if (s[pos] == '\\' && pos + 1 < n && s[pos + 1] == '"') { put('"'); pos += 2; continue; }
      put(s[pos++]);
    }
    if (pos < n) pos++;                       // salta la virgoletta finale
  } else {
    while (pos < n && s[pos] != ' ') put(s[pos++]);
  }
  if (cap) out[w] = 0;
  return true;
}

bool utilHostOk(std::string_view s) {
  if (s.size() < 1 || s.size() > 60) return false;
  for (char ch : s) { if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '.' || ch == '-')) return false; }
  return true;
}

// Mai prima del 1 gennaio 2026; dopo una sincronizzazione buona, un salto di piu di un giorno e sospetto
// (si accetta comunque alla quarta volta di fila: vuol dire che l'ora vera e cambiata davvero)
bool utilTimeSyncOk(uint32_t got, uint32_t expected, uint8_t refused) {
  if (got < 1767225600UL) return false;
  if (expected && refused < 3) { uint32_t d = got > expected ? got - expected : expected - got; if (d > 86400UL) return false; }
  return true;
}

// Quanto stack si puo togliere a un task: il libero minimo visto, meno il 25% dello stack totale tenuto di scorta
uint32_t utilStackSpare(uint32_t total, uint32_t freeMin) {
  if (!total) return 0;
  uint32_t keep = total / 4;
  if (freeMin <= keep) return 0;
  return ((freeMin - keep) / 256U) * 256U;
}

// ---------- SVG ammesso: solo forme semplici (icone delle app e delle schede modulo) ----------
static char lc(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }
static bool lcEq(std::string_view v, const char* lit) {            // uguale a lit, senza badare alle maiuscole
  size_t n = strlen(lit);
  if (v.size() != n) return false;
  for (size_t i = 0; i < n; i++) if (lc(v[i]) != lit[i]) return false;
  return true;
}
static bool lcHas(std::string_view v, const char* lit) {           // contiene lit, senza badare alle maiuscole
  size_t n = strlen(lit);
  for (size_t i = 0; i + n <= v.size(); i++) if (lcEq(v.substr(i, n), lit)) return true;
  return false;
}
static bool svgTagAllowed(std::string_view t) {
  static const char* ok[] = {"svg", "g", "path", "circle", "rect", "ellipse", "line", "polyline", "polygon", "title"};
  for (const char* o : ok) if (lcEq(t, o)) return true;
  return false;
}
bool utilSvgSafe(std::string_view l, size_t maxLen) {
  if (l.size() < 10 || l.size() > maxLen) return false;
  if (!lcEq(l.substr(0, 4), "<svg") || !lcEq(l.substr(l.size() - 6), "</svg>")) return false;
  static const char* bad[] = {"script", "href", "url(", "data:", "style", "@import", "&", "<!", "<?", "expression", "base64", "xlink"};
  for (const char* b : bad) if (lcHas(l, b)) return false;
  size_t i = 0, n = l.size();
  int tags = 0;
  while (i < n) {
    if (l[i] != '<') { i++; continue; }
    size_t j = i + 1; bool closing = false;
    if (j < n && l[j] == '/') { closing = true; j++; }
    size_t n0 = j;
    while (j < n && l[j] != ' ' && l[j] != '/' && l[j] != '>' && l[j] != '\n' && l[j] != '\t' && l[j] != '\r') j++;
    if (!svgTagAllowed(l.substr(n0, j - n0)) || ++tags > 60) return false;
    if (closing) { while (j < n && l[j] != '>') j++; i = j + 1; continue; }
    // attributi: nessun nome che inizi per "on" (onclick, onload...)
    while (j < n && l[j] != '>') {
      while (j < n && (l[j] == ' ' || l[j] == '\n' || l[j] == '\t' || l[j] == '\r' || l[j] == '/')) j++;
      if (j >= n || l[j] == '>') break;
      size_t a0 = j;
      while (j < n && l[j] != '=' && l[j] != ' ' && l[j] != '>' && l[j] != '/' && l[j] != '\n' && l[j] != '\t' && l[j] != '\r') j++;
      std::string_view an = l.substr(a0, j - a0);
      if (an.empty() || (an.size() >= 2 && lc(an[0]) == 'o' && lc(an[1]) == 'n')) return false;
      if (j < n && l[j] == '=') {
        j++;
        if (j < n && (l[j] == '"' || l[j] == '\'')) { char q = l[j++]; while (j < n && l[j] != q) j++; if (j >= n) return false; j++; }
        else { while (j < n && l[j] != ' ' && l[j] != '>') j++; }
      }
    }
    if (j >= n) return false;
    i = j + 1;
  }
  return tags >= 2;
}

