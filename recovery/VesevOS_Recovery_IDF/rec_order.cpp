// VesevOS Recovery - ordine di aggiornamento e esito (vedi rec_order.h).
#include "rec_order.h"
#include "rec_util.h"
#include <stdio.h>
#include <string.h>
#include <nvs.h>

#define NS "vosupd"
#define MAX_WRONG 3

static RecOrder g_order;
static bool g_expiredLogged = false;

RecOrder& recOrder() { return g_order; }
std::string recOrderVer() { return g_order.ver; }

static bool getStr(nvs_handle_t h, const char* key, size_t maxLen, std::string& out) {
  size_t n = 0;
  if (nvs_get_str(h, key, nullptr, &n) != ESP_OK || n < 2 || n > maxLen) return false;
  out.assign(n, '\0');
  if (nvs_get_str(h, key, &out[0], &n) != ESP_OK) return false;
  out.resize(n - 1);
  return true;
}

static bool getBlob(nvs_handle_t h, const char* key, uint8_t* out, size_t want) {
  size_t n = 0;
  if (nvs_get_blob(h, key, nullptr, &n) != ESP_OK || n != want) return false;
  return nvs_get_blob(h, key, out, &n) == ESP_OK;
}

static bool printableAscii(const std::string& s) {
  for (char c : s) if (c < 0x21 || c > 0x7E) return false;
  return true;
}

// Cancella tutte le chiavi dell'ordine (l'esito resta).
static void eraseOrder(nvs_handle_t h) {
  static const char* keys[] = {"mode", "url", "sha", "ver", "size", "token", "cip", "sig", "exp"};
  for (const char* k : keys) nvs_erase_key(h, k);
  nvs_commit(h);
}

void recResult(int code, const std::string& msg) {
  nvs_handle_t h;
  if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) return;
  nvs_set_u8(h, "res", (uint8_t)code);
  nvs_set_str(h, "rmsg", msg.substr(0, 40).c_str());
  nvs_set_str(h, "rver", g_order.ver.substr(0, 24).c_str());
  nvs_commit(h);
  nvs_close(h);
  printf("[recovery] esito dell'ordine: %d (%s)\n", code, msg.c_str());
}

void recOrderLoad() {
  nvs_handle_t h;
  if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) return;       // nessuno spazio = nessun ordine
  uint8_t mode = 0;
  if (nvs_get_u8(h, "mode", &mode) != ESP_OK || mode == 0) { nvs_close(h); return; }
  RecOrder o;
  o.mode = mode;
  bool ok = (mode == 1 || mode == 2);
  o.haveSha = getBlob(h, "sha", o.sha, 32);                     // obbligatorio nel modo 1, facoltativo nel modo 2
  if (ok && mode == 1) {
    ok = getStr(h, "url", 200, o.url) && o.url.compare(0, 8, "https://") == 0 && printableAscii(o.url) && o.haveSha;
  } else if (ok) {
    ok = getBlob(h, "token", o.token, 16);
    nvs_get_u32(h, "cip", &o.cip);
  }
  if (ok) {
    getStr(h, "ver", 24, o.ver);
    if (!printableAscii(o.ver)) o.ver.clear();
    nvs_get_u32(h, "size", &o.size);
    uint8_t minutes = 15;
    if (nvs_get_u8(h, "exp", &minutes) != ESP_OK || minutes < 1 || minutes > 60) minutes = 15;
    o.t0 = recMillis();
    o.expMs = (uint32_t)minutes * 60000UL;
  }
  eraseOrder(h);                                                 // uso singolo: via subito, anche se poi qualcosa va storto
  nvs_close(h);
  if (!ok) { g_order = RecOrder(); recResult(RES_ORDER, "ordine non valido"); return; }
  g_order = o;
  printf("[recovery] ordine di aggiornamento: modo %d, versione %s, valido %u minuti\n", o.mode, o.ver.c_str(), (unsigned)(o.expMs / 60000UL));
}

bool recOrderExpired() {
  if (g_order.mode == 0) return false;
  if (recMillis() - g_order.t0 < g_order.expMs) return false;
  if (!g_expiredLogged) { g_expiredLogged = true; if (!g_order.burnt) recResult(RES_EXPIRED, "ordine scaduto"); g_order.burnt = true; }
  return true;
}

bool recGrantCheck(const std::string& tokHex, uint32_t ip) {
  RecOrder& o = g_order;
  if (o.mode != 2 || o.burnt || recOrderExpired() || tokHex.empty()) return false;
  bool tokOk = sameStr(tokHex, hexOf(o.token, 16));
  bool ipOk = (o.cip == 0 || o.cip == ip);
  if (tokOk && ipOk) return true;
  if (++o.wrong >= MAX_WRONG) { o.burnt = true; recResult(RES_CANCEL, "troppi tentativi"); }
  return false;
}

void recGrantConsume() { g_order.burnt = true; g_order.used = true; }

std::string recGrantDenyText() {
  const char* again = "riaprilo da VesevOS oppure entra con la password";
  if (g_order.mode != 2) return std::string("Serve il login: non c'e un link di VesevOS valido (") + again + ")";
  if (recOrderExpired()) return std::string("Il link di VesevOS e scaduto (dura ") + std::to_string(g_order.expMs / 60000UL) + " minuti): " + again;
  if (g_order.used) return std::string("Il link di VesevOS e gia stato usato (vale una volta sola): ") + again;
  if (g_order.burnt) return std::string("Il link di VesevOS e stato annullato per troppi tentativi sbagliati: ") + again;
  return std::string("Il link di VesevOS non e corretto: ") + again;
}
