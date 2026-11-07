// VesevOS Recovery - ordine di aggiornamento e esito (vedi rec_order.h).
#include "rec_order.h"
#include "rec_sd.h"
#include "rec_util.h"
#include <stdio.h>
#include <string.h>
#include <nvs.h>

#define NS "vosupd"
#define MAX_WRONG 3
#define SD_TENTATIVI 2

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

// Modo 3: percorso corto, senza salite.
static bool sdFileOk(const std::string& f) {
  return f.size() >= 2 && f.size() <= 15 && f[0] == '/' && f.find("..") == std::string::npos && printableAscii(f);
}

// Modo 3: niente pin della flash (26-32) o della USB (19, 20), niente doppioni.
// Stessa regola in VesevOS/src/core/vos_updorder.h (updPinSdOk): se cambi qui, cambia anche li.
static bool sdPinsOk(const uint8_t* p) {
  for (int i = 0; i < 4; i++) {
    if (p[i] > 48 || p[i] == 19 || p[i] == 20 || (p[i] >= 26 && p[i] <= 32)) return false;
    for (int j = 0; j < i; j++) if (p[i] == p[j]) return false;
  }
  return true;
}

// Cancella tutte le chiavi dell'ordine (l'esito resta).
static void eraseOrder(nvs_handle_t h) {
  static const char* keys[] = {"mode", "url", "sha", "ver", "size", "token", "cip", "sig", "exp", "auto", "sdf", "sdp", "khz", "prv"};
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
  uint8_t automatico = 0;
  nvs_get_u8(h, "auto", &automatico);
  o.automatico = (mode == 1 && automatico == 1) || mode == 3;
  bool ok = (mode >= 1 && mode <= 3);
  o.haveSha = getBlob(h, "sha", o.sha, 32);                     // obbligatorio nel modo 1, facoltativo nel modo 2
  if (ok && mode == 1) {
    ok = getStr(h, "url", 200, o.url) && o.url.compare(0, 8, "https://") == 0 && printableAscii(o.url) && o.haveSha;
  } else if (ok && mode == 3) {
    ok = getStr(h, "sdf", 16, o.sdFile) && sdFileOk(o.sdFile) && o.haveSha && getBlob(h, "sdp", o.sdPin, 4) && sdPinsOk(o.sdPin);
    if (nvs_get_u32(h, "khz", &o.sdKhz) != ESP_OK || o.sdKhz < 400 || o.sdKhz > 20000) o.sdKhz = 4000;
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
  if (ok && mode == 3) {
    uint8_t fatti = 0;
    nvs_get_u8(h, "prv", &fatti);
    if (fatti < SD_TENTATIVI && o.size) {                        // resta nella memoria: se manca la corrente si riparte da qui
      nvs_close(h);
      g_order = o;
      printf("[recovery] ordine di aggiornamento: dalla microSD, versione %s, tentativi fatti %u\n", o.ver.c_str(), (unsigned)fatti);
      return;
    }
    eraseOrder(h);                                               // basta: niente giri a vuoto, resta la pagina
    nvs_close(h);
    g_order = RecOrder();
    g_order.ver = o.ver;
    recResult(RES_WRITE, "troppi tentativi dalla microSD");
    return;
  }
  eraseOrder(h);                                                 // uso singolo: via subito, anche se poi qualcosa va storto
  nvs_close(h);
  if (!ok) {
    g_order = RecOrder();
    g_order.automatico = o.automatico;                           // anche un ordine rifiutato deve tornare a VesevOS
    recResult(RES_ORDER, "ordine non valido");
    return;
  }
  g_order = o;
  printf("[recovery] ordine di aggiornamento: modo %d, versione %s, valido %u minuti\n", o.mode, o.ver.c_str(), (unsigned)(o.expMs / 60000UL));
}

bool recSdFileOk(const RecSdFile& f) { return sdFileOk(f.file) && sdPinsOk(f.pin); }

void recOrderTentativo() {
  nvs_handle_t h;
  if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) return;
  uint8_t fatti = 0;
  nvs_get_u8(h, "prv", &fatti);
  nvs_set_u8(h, "prv", (uint8_t)(fatti + 1));
  nvs_commit(h);
  nvs_close(h);
}

void recOrderChiudi() {
  nvs_handle_t h;
  if (nvs_open(NS, NVS_READWRITE, &h) == ESP_OK) { eraseOrder(h); nvs_close(h); }
  g_order.mode = 0;
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
