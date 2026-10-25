// VesevOS Recovery - accesso (vedi rec_auth.h). Stessa formula e stessi testi della 0.4.4.
#include "rec_auth.h"
#include "rec_config.h"
#include "rec_util.h"
#include "rec_version.h"
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static std::string g_code, g_tok, g_nonce, g_nonceUser;
static uint32_t g_tokT = 0, g_nonceT = 0, g_lockUntil = 0;
static uint8_t g_fails = 0;

void recAuthNewCode() { g_code = std::to_string((unsigned long)(10000000UL + (esp_random() % 90000000UL))); }   // 8 cifre
const std::string& recAuthCode() { return g_code; }

std::string recJsonErr(const std::string& e) { return "{\"ok\":false,\"err\":\"" + e + "\"}"; }

static bool locked(std::string& msg) {
  if (g_lockUntil && (int32_t)(g_lockUntil - recMillis()) > 0) {
    msg = "Troppi tentativi: riprova tra " + std::to_string((g_lockUntil - recMillis()) / 1000 + 1) + " s";
    return true;
  }
  return false;
}

static void failOne() {
  vTaskDelay(pdMS_TO_TICKS(800));                     // rallenta chi prova a indovinare
  if (++g_fails >= 5) { g_fails = 0; g_lockUntil = recMillis() + 60000UL; }
}

static RecReply okLogin() {
  g_fails = 0; g_lockUntil = 0;
  g_tok = randomHex(16); g_tokT = recMillis();
  return {200, "{\"ok\":true,\"t\":\"" + g_tok + "\"}"};
}

bool recAuthed(const std::string& token) {
  if (g_tok.empty() || recMillis() - g_tokT > TOKEN_MS) return false;
  return sameStr(token, g_tok);
}

RecReply recAuthVer() {
  return {200, std::string("{\"rec\":\"") + REC_VERSION + "\",\"code\":" + (rc.adminOk ? "false" : "true") + "}"};
}

RecReply recAuthStart(const std::string& u) {
  std::string m;
  if (locked(m)) return {429, recJsonErr(m)};
  int i = userFind(u);
  std::string salt = (i >= 0) ? rc.users[i].salt : sha256Hex(g_code + "x" + u).substr(0, 32);   // nome sconosciuto: sale finto ma stabile
  g_nonce = randomHex(16); g_nonceUser = u; g_nonceT = recMillis();
  return {200, "{\"ok\":true,\"salt\":\"" + salt + "\",\"nonce\":\"" + g_nonce + "\",\"iter\":" + std::to_string(HASH_ITER) + "}"};
}

RecReply recAuthLogin(const std::string& form) {
  std::string m;
  if (locked(m)) return {429, recJsonErr(m)};
  if (!rc.adminOk) {                                  // flash vuota: codice dalla seriale
    if (sameStr(formGet(form, "code"), g_code)) return okLogin();
    failOne();
    return {401, recJsonErr("Codice errato")};
  }
  std::string u = formGet(form, "u"), nonce = formGet(form, "nonce"), mac = formGet(form, "mac");
  bool fresh = g_nonce.size() && recMillis() - g_nonceT < NONCE_MS && sameStr(nonce, g_nonce) && u == g_nonceUser;
  g_nonce.clear();                                    // il numero casuale vale una volta sola
  int i = userFind(u);
  bool ok = fresh && i >= 0 && rc.users[i].on && rc.users[i].role == 2 && rc.users[i].hash.size() == 64 &&
            sameStr(hmacHex(rc.users[i].hash, nonce), mac);
  if (ok) return okLogin();
  failOne();
  return {401, recJsonErr("Utente o password errati (serve un amministratore)")};
}
