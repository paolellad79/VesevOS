// VesevOS Recovery 0.4.4 - strada B2: recupero con Wi-Fi + controlli di sicurezza sul file + LED di stato + file trascinabile sulla pagina + diagnostica dell'avvio.
// Licenza: GPL-3.0 (come VesevOS).
// Cosa fa:
//  - Se VesevOS e valido e nessuno ha chiesto il recovery: dopo 3 s passa a VesevOS (come la 0.1.x).
//  - Se VesevOS ha chiesto il recovery (comando `recovery now`), oppure app0 non e valido, oppure BOOT premuto all'accensione:
//    resta qui, si collega alla rete di casa (impostazioni lette dal file di VesevOS in LittleFS) e, se non ci riesce in 30 s,
//    apre il proprio hotspot. Mostra una pagina dove, con utente e password di un amministratore di VesevOS, carichi il .bin.
//  - Se non esiste nessun amministratore (flash vuota): serve il codice a 8 cifre mostrato sulla seriale.
// Controlli prima di attivare un firmware (passo 3): intestazione ESP32-S3, marchio VesevOS, dimensione, SHA-256 (facoltativo,
// se lo scrivi nella pagina), immagine completa (esp_ota_end). Se l'intestazione e sbagliata il vecchio firmware NON viene toccato.
// LED di stato (stesso LED RGB di VesevOS, pin e luminosita letti da /vesevos.conf): arancione lento = in attesa; azzurro veloce = scrittura
// in corso; verde fisso = finito; rosso fisso = errore (dopo 10 s torna in attesa). Con VesevOS valido e in avvio il LED resta spento.
// Diagnostica (0.4.1): all'avvio e con "i" scrive il motivo dell'ultimo riavvio, quale partizione ha scelto il bootloader, lo stato di app0 e il segno
// "passaggio a VesevOS". Se l'ultima volta il passaggio a VesevOS e finito con un ritorno al recovery, il recovery RESTA fermo (niente giri a vuoto).
// Protezione (0.4.3): prima di passare ad app0 il recovery cerca nel suo contenuto il marchio VesevOS (lo stesso controllo del file caricato).
// Se manca (per esempio nello slot c'e un altro recovery o un file sbagliato) NON passa e resta fermo. Il comando seriale b forza il passaggio.
// Marchio del recovery (0.4.4): VesevOS 1.7.58+ lo cerca quando aggiorni il recovery dalla pagina (Aggiorna recovery) e ne legge la versione.
// Senza marchio VesevOS rifiuta il file: cosi il firmware non puo finire al posto del recovery.
// Comandi sulla seriale (115200): i = info, b = avvia VesevOS (anche senza marchio), r = riavvia.
// Arduino IDE: stessa scheda del firmware; "USB CDC On Boot" = Enabled. Tabella partizioni: partitions.csv accanto allo sketch.
// Limite di spazio del recovery: 1 MB (vedi partitions.csv): lo script di installazione controlla la dimensione.
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_system.h>
#include <esp_random.h>
#include <mbedtls/sha256.h>
#include <mbedtls/md.h>
#include "recovery_page.h"
#include "recovery_check.h"

#define REC_VERSION "0.4.4"
#define BOOT_PIN 0
// non cambiare il formato "{REC:VesevOS:" + versione + "}": vedi VesevOS/src/core/vos_imgcheck.h
static const char REC_MARK[] __attribute__((used)) = "{REC:VesevOS:" REC_VERSION "}";
#define HANDOVER_MS 3000
#define STA_WAIT_MS 30000UL
#define IDLE_MS 900000UL        // 15 minuti senza accessi: se VesevOS e valido, ci torna da solo
#define TOKEN_MS 600000UL
#define NONCE_MS 60000UL
#define HASH_ITER 3000

// ---------- utilita ----------
static String hexOf(const unsigned char* b, size_t n) {
  static const char* hx = "0123456789abcdef";
  String r; r.reserve(n * 2);
  for (size_t i = 0; i < n; i++) { r += hx[b[i] >> 4]; r += hx[b[i] & 15]; }
  return r;
}
static String sha256Hex(const String& s) {
  unsigned char out[32];
  mbedtls_sha256_context c;
  mbedtls_sha256_init(&c);
  mbedtls_sha256_starts(&c, 0);
  mbedtls_sha256_update(&c, (const unsigned char*)s.c_str(), s.length());
  mbedtls_sha256_finish(&c, out);
  mbedtls_sha256_free(&c);
  return hexOf(out, 32);
}
static String hmacHex(const String& key, const String& msg) {
  unsigned char out[32];
  const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (!md || mbedtls_md_hmac(md, (const unsigned char*)key.c_str(), key.length(), (const unsigned char*)msg.c_str(), msg.length(), out) != 0) return "";
  return hexOf(out, 32);
}
static String randomHex(int bytes) {
  unsigned char b[32];
  if (bytes > 32) bytes = 32;
  for (int i = 0; i < bytes; i++) b[i] = (unsigned char)(esp_random() & 0xFF);
  return hexOf(b, bytes);
}
static bool sameStr(const String& a, const String& b) {   // confronto a tempo costante
  if (a.length() != b.length()) return false;
  unsigned char d = 0;
  for (size_t i = 0; i < a.length(); i++) d |= (unsigned char)(a[i] ^ b[i]);
  return d == 0;
}

// ---------- configurazione di VesevOS (file /vesevos.conf in LittleFS) ----------
struct RecUser { String name, salt, hash; int role = 0; bool on = false; };
struct RecCfg {
  int ledPin = 48, ledBr = 80;      // LED RGB integrato (WS2812), come in VesevOS
  String host = "vesevos", ssid, pass, ip, mask, gw, dns1, dns2, country;
  bool staOn = false, dhcp = true;
  RecUser users[8];
  int nUsers = 0;
  bool adminOk = false;
} rc;

// valore tra apici con la regola OpenWrt: ' diventa '\''
static String parseQuoted(const String& l, int p) {
  String v;
  while (p < (int)l.length() && l[p] != '\'') p++;
  if (p >= (int)l.length()) return v;
  p++;
  while (p < (int)l.length()) {
    if (l[p] == '\'') {
      if (p + 3 < (int)l.length() + 0 && l[p + 1] == '\\' && l[p + 2] == '\'' && l[p + 3] == '\'') { v += '\''; p += 4; continue; }
      break;
    }
    v += l[p++];
  }
  return v;
}

static void loadConfig() {
  if (!LittleFS.begin(false, "/littlefs", 10, "spiffs")) { Serial.println("[recovery] LittleFS non montato: nessuna configurazione"); return; }
  File f = LittleFS.open("/vesevos.conf", "r");
  if (!f) { Serial.println("[recovery] /vesevos.conf non trovato"); LittleFS.end(); return; }
  String sec, type;
  int ui = -1;
  while (f.available()) {
    String l = f.readStringUntil('\n');
    l.trim();
    if (l.startsWith("config ")) {
      type = l.substring(7, l.indexOf(' ', 7) > 0 ? l.indexOf(' ', 7) : l.length());
      sec = parseQuoted(l, 7);
      ui = -1;
      if (type == "user" && rc.nUsers < 8) { ui = rc.nUsers++; }
      continue;
    }
    if (!l.startsWith("option ")) continue;
    int sp = l.indexOf(' ', 7);
    if (sp < 0) continue;
    String k = l.substring(7, sp);
    String v = parseQuoted(l, sp);
    if (type == "system") { if (k == "hostname" && v.length()) rc.host = v; }
    else if (type == "region") { if (k == "country") rc.country = v; }
    else if (type == "led") {
      if (k == "pin") { int n = v.toInt(); if (n >= 0 && n <= 48) rc.ledPin = n; }
      else if (k == "brightness") { int n = v.toInt(); if (n >= 0 && n <= 255) rc.ledBr = n < 40 ? 40 : n; }
    }
    else if (type == "client") {
      if (k == "enabled") rc.staOn = (v == "1");
      else if (k == "ssid") rc.ssid = v;
      else if (k == "pass") rc.pass = v;
      else if (k == "dhcp") rc.dhcp = (v != "0");
      else if (k == "ip") rc.ip = v; else if (k == "mask") rc.mask = v; else if (k == "gw") rc.gw = v;
      else if (k == "dns1") rc.dns1 = v; else if (k == "dns2") rc.dns2 = v;
    } else if (type == "user" && ui >= 0) {
      RecUser& u = rc.users[ui];
      if (k == "name") u.name = v;
      else if (k == "role") u.role = v.toInt();
      else if (k == "on") u.on = (v == "1");
      else if (k == "salt") u.salt = v;
      else if (k == "hash") u.hash = v;
    }
  }
  f.close();
  LittleFS.end();
  for (int i = 0; i < rc.nUsers; i++) if (rc.users[i].name.length() && rc.users[i].on && rc.users[i].role == 2 && rc.users[i].hash.length() == 64) rc.adminOk = true;
  Serial.printf("[recovery] configurazione letta: rete di casa %s, amministratori %s\n", (rc.staOn && rc.ssid.length()) ? "impostata" : "non impostata", rc.adminOk ? "presenti" : "NESSUNO");
}

static int userFind(const String& n) {
  for (int i = 0; i < rc.nUsers; i++) if (rc.users[i].name.length() && rc.users[i].name == n) return i;
  return -1;
}

// ---------- LED di stato ----------
enum { LS_WAIT, LS_WRITE, LS_DONE, LS_ERR };
static uint8_t g_ledSt = LS_WAIT;
static bool g_ledOn = false;
static uint32_t g_ledErrT = 0, g_ledKey = 0xFFFFFFFFUL;
static void ledTick() {
  if (!g_ledOn) return;
  uint32_t n = millis();
  if (g_ledSt == LS_ERR && n - g_ledErrT > 10000UL) g_ledSt = LS_WAIT;
  uint8_t r = 0, g = 0, b = 0;
  switch (g_ledSt) {
    case LS_WAIT:  if ((n % 1600UL) < 800UL) { r = 255; g = 120; } break;          // arancione lento
    case LS_WRITE: if ((n / 150UL) & 1) { g = 160; b = 255; } break;                // azzurro veloce
    case LS_DONE:  g = 255; break;                                                  // verde fisso
    default:       r = 255; break;                                                  // rosso fisso
  }
  r = (uint8_t)((uint16_t)r * rc.ledBr / 255); g = (uint8_t)((uint16_t)g * rc.ledBr / 255); b = (uint8_t)((uint16_t)b * rc.ledBr / 255);
  uint32_t key = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
  if (key != g_ledKey) { g_ledKey = key; rgbLedWrite(rc.ledPin, r, g, b); }
}
static void ledSet(uint8_t st) { g_ledSt = st; if (st == LS_ERR) g_ledErrT = millis(); ledTick(); }

// ---------- partizioni / avvio ----------
static const esp_partition_t* app0() {
  return esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
}
static bool app0Valid(char* name, size_t nl, char* ver, size_t vl) {
  const esp_partition_t* p = app0();
  if (!p) return false;
  uint8_t b = 0;
  if (esp_partition_read(p, 0, &b, 1) != ESP_OK || b != 0xE9) return false;
  esp_app_desc_t d;
  if (esp_ota_get_partition_description(p, &d) != ESP_OK) return false;
  snprintf(name, nl, "%s", d.project_name);
  snprintf(ver, vl, "%s", d.version);
  return true;
}
// Il firmware dentro app0 e VesevOS? Si cerca il marchio (mascherato, come per il file caricato). Risultato ricordato: -1 = da controllare.
static int8_t g_fwMark = -1;
static bool partRead(void* ctx, uint32_t off, uint8_t* buf, size_t n) { return esp_partition_read((const esp_partition_t*)ctx, off, buf, n) == ESP_OK; }
static bool app0Fw() {
  if (g_fwMark >= 0) return g_fwMark == 1;
  const esp_partition_t* p = app0();
  static uint8_t buf[2048];
  g_fwMark = (p && rcScanMark(partRead, (void*)p, p->size, buf, sizeof buf)) ? 1 : 0;
  return g_fwMark == 1;
}
static bool stayFlag(bool set, bool val) {      // segno lasciato da VesevOS ("recovery now")
  Preferences p;
  if (!p.begin("vosrec", set ? false : true)) return false;
  bool r;
  if (set) { p.putBool("stay", val); r = val; } else r = p.getBool("stay", false);
  p.end();
  return r;
}
static bool handFlag(bool set, bool val) {      // segno "sto passando a VesevOS": se al riavvio e ancora acceso, VesevOS non e partito
  Preferences p;
  if (!p.begin("vosrec", set ? false : true)) return false;
  bool r;
  if (set) { p.putBool("hand", val); r = val; } else r = p.getBool("hand", false);
  p.end();
  return r;
}
static void startApp() {
  stayFlag(true, false);
  handFlag(true, true);
  const esp_partition_t* p = app0();
  if (!p) { Serial.println("[recovery] app0 non trovata"); return; }
  esp_err_t e = esp_ota_set_boot_partition(p);
  if (e != ESP_OK) { Serial.printf("[recovery] errore nel passare ad app0: %d\n", (int)e); return; }
  Serial.println("[recovery] passo a VesevOS...");
  Serial.flush();
  delay(100);
  esp_restart();
}

// ---------- rete ----------
static WebServer server(80);
static bool g_net = false, g_ap = false;
static String g_apPass, g_code;
static String g_tok, g_nonce, g_nonceUser;
static uint32_t g_tokT = 0, g_nonceT = 0, g_lockUntil = 0, g_lastAct = 0;
static uint8_t g_fails = 0;
static bool g_upOk = false;
static String g_upErr;

static String ipStr() { return g_ap ? WiFi.softAPIP().toString() : WiFi.localIP().toString(); }

static void applyCountry() {
  if (rc.country.length() == 2) esp_wifi_set_country_code(rc.country.c_str(), true);
}

static void netInfo() {
  if (!g_net) { Serial.println("[recovery] rete non avviata"); return; }
  if (g_ap) Serial.printf("[recovery] HOTSPOT 'VesevOS-recovery' password %s - apri http://%s\n", g_apPass.c_str(), ipStr().c_str());
  else Serial.printf("[recovery] rete di casa: apri http://%s oppure http://%s.local\n", ipStr().c_str(), rc.host.c_str());
  if (!rc.adminOk) Serial.printf("[recovery] nessun amministratore: nella pagina scrivi il codice %s\n", g_code.c_str());
}

static bool authed() {
  if (!g_tok.length() || millis() - g_tokT > TOKEN_MS) return false;
  return sameStr(server.header("X-T"), g_tok);
}
static void jsonOut(int code, const String& j) {
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("X-Frame-Options", "DENY");
  server.send(code, "application/json", j);
}
static String jerr(const String& e) { return "{\"ok\":false,\"err\":\"" + e + "\"}"; }
static bool locked(String& msg) {
  if (g_lockUntil && (int32_t)(g_lockUntil - millis()) > 0) { msg = "Troppi tentativi: riprova tra " + String((g_lockUntil - millis()) / 1000 + 1) + " s"; return true; }
  return false;
}
static void failOne() {
  delay(800);
  if (++g_fails >= 5) { g_fails = 0; g_lockUntil = millis() + 60000UL; }
}

static void hRoot() {
  g_lastAct = millis();
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("X-Frame-Options", "DENY");
  server.send_P(200, "text/html", REC_PAGE);
}
static void hVer() { jsonOut(200, String("{\"rec\":\"") + REC_VERSION + "\",\"code\":" + (rc.adminOk ? "false" : "true") + "}"); }
static void hStart() {
  g_lastAct = millis();
  String m;
  if (locked(m)) { jsonOut(429, jerr(m)); return; }
  String u = server.arg("u");
  int i = userFind(u);
  String salt = (i >= 0) ? rc.users[i].salt : sha256Hex(g_code + "x" + u).substring(0, 32);   // nome sconosciuto: sale finto ma stabile
  g_nonce = randomHex(16); g_nonceUser = u; g_nonceT = millis();
  jsonOut(200, "{\"ok\":true,\"salt\":\"" + salt + "\",\"nonce\":\"" + g_nonce + "\",\"iter\":" + String(HASH_ITER) + "}");
}
static void okLogin() {
  g_fails = 0; g_lockUntil = 0;
  g_tok = randomHex(16); g_tokT = millis();
  jsonOut(200, "{\"ok\":true,\"t\":\"" + g_tok + "\"}");
}
static void hLogin() {
  g_lastAct = millis();
  String m;
  if (locked(m)) { jsonOut(429, jerr(m)); return; }
  if (!rc.adminOk) {                                  // flash vuota: codice dalla seriale
    if (sameStr(server.arg("code"), g_code)) { okLogin(); return; }
    failOne(); jsonOut(401, jerr("Codice errato")); return;
  }
  String u = server.arg("u"), nonce = server.arg("nonce"), mac = server.arg("mac");
  bool fresh = g_nonce.length() && millis() - g_nonceT < NONCE_MS && sameStr(nonce, g_nonce) && u == g_nonceUser;
  g_nonce = "";                                       // il numero casuale vale una volta sola
  int i = userFind(u);
  bool ok = fresh && i >= 0 && rc.users[i].on && rc.users[i].role == 2 && rc.users[i].hash.length() == 64 && sameStr(hmacHex(rc.users[i].hash, nonce), mac);
  if (ok) { okLogin(); return; }
  failOne();
  jsonOut(401, jerr("Utente o password errati (serve un amministratore)"));
}
static void hInfo() {
  if (!authed()) { jsonOut(401, jerr("Non autorizzato")); return; }
  g_lastAct = millis();
  char n[40], v[40];
  String a = app0Valid(n, sizeof n, v, sizeof v) ? (app0Fw() ? String(n) + " " + String(v) : String("NON e VesevOS (manca il marchio)")) : String("");
  const esp_partition_t* p = app0();
  jsonOut(200, "{\"ok\":true,\"app\":\"" + a + "\",\"max\":" + String(p ? (unsigned long)p->size : 0UL) + "}");
}
static void hBoot() {
  if (!authed()) { jsonOut(401, jerr("Non autorizzato")); return; }
  char n[40], v[40];
  if (!app0Valid(n, sizeof n, v, sizeof v)) { jsonOut(409, jerr("Non c'e un firmware valido da avviare")); return; }
  if (!app0Fw()) { jsonOut(409, jerr("Nello slot non c'e VesevOS (manca il marchio): carica il firmware")); return; }
  jsonOut(200, "{\"ok\":true}");
  delay(300);
  startApp();
}
static esp_ota_handle_t g_oh = 0;
static bool g_oBegun = false;
static RecCheck g_chk;
static mbedtls_sha256_context g_sha;
static bool g_shaOn = false;
static String g_upSha;

static void shaStop() { if (g_shaOn) { mbedtls_sha256_free(&g_sha); g_shaOn = false; } }
// Errore durante il caricamento: se avevamo gia cominciato a scrivere, il vecchio firmware e perso -> lo rendiamo non valido
// (primo settore cancellato) cosi il recovery non prova ad avviare un'immagine a meta.
static void upFail(const String& why) {
  g_upOk = false; g_upErr = why;
  ledSet(LS_ERR);
  if (g_oBegun) {
    esp_ota_abort(g_oh);
    const esp_partition_t* p = app0();
    if (p) esp_partition_erase_range(p, 0, 4096);
    g_oBegun = false;
  }
  shaStop();
}
static void hUploadDone() {
  if (g_upOk) {
    stayFlag(true, false);
    ledSet(LS_DONE);
    jsonOut(200, "{\"ok\":true,\"sha\":\"" + g_upSha + "\"}");
    Serial.println("[recovery] firmware installato: riavvio");
    delay(600);
    esp_restart();
  } else jsonOut(g_upErr == "Non autorizzato" ? 401 : 400, jerr(g_upErr.length() ? g_upErr : "Caricamento non riuscito"));
}
static void hUpload() {
  HTTPUpload& u = server.upload();
  if (u.status == UPLOAD_FILE_START) {
    g_lastAct = millis();
    shaStop();
    g_oBegun = false; g_upSha = ""; g_fwMark = -1;
    g_upOk = authed(); g_upErr = g_upOk ? "" : "Non autorizzato";
    if (g_upOk) {
      Serial.printf("[recovery] ricevo %s\n", u.filename.c_str());
      ledSet(LS_WRITE);
      rcInit(g_chk);
      mbedtls_sha256_init(&g_sha); mbedtls_sha256_starts(&g_sha, 0); g_shaOn = true;
    }
  } else if (u.status == UPLOAD_FILE_WRITE) {
    if (!g_upOk) return;
    ledTick();
    rcFeed(g_chk, u.buf, u.currentSize);
    mbedtls_sha256_update(&g_sha, u.buf, u.currentSize);
    if (!g_oBegun) {                                  // primo pezzo: controllo l'intestazione PRIMA di cancellare qualcosa
      const char* e = (g_chk.nh >= RC_HDR) ? rcHeaderErr(g_chk) : "File troppo piccolo: non e un firmware";
      if (e) { upFail(e); Serial.printf("[recovery] file rifiutato: %s\n", e); return; }
      const esp_partition_t* p = app0();
      if (!p) { upFail("Partizione del firmware non trovata"); return; }
      if (esp_ota_begin(p, OTA_SIZE_UNKNOWN, &g_oh) != ESP_OK) { upFail("Non posso scrivere nella partizione"); return; }
      g_oBegun = true;
    }
    if (esp_ota_write(g_oh, u.buf, u.currentSize) != ESP_OK) upFail("Scrittura fallita (file troppo grande?)");
  } else if (u.status == UPLOAD_FILE_END) {
    if (!g_upOk) return;
    unsigned char dg[32];
    mbedtls_sha256_finish(&g_sha, dg);
    g_upSha = hexOf(dg, 32);
    const esp_partition_t* p = app0();
    const char* e = rcFinalErr(g_chk, p ? p->size : 0);
    if (e) { upFail(e); Serial.printf("[recovery] file rifiutato: %s\n", e); return; }
    String want = server.header("X-Sha"); want.trim(); want.toLowerCase();
    if (want.length() && want != g_upSha) { upFail("SHA-256 diverso da quello atteso: file danneggiato o sbagliato"); Serial.printf("[recovery] SHA-256 ricevuto %s\n", g_upSha.c_str()); return; }
    if (esp_ota_end(g_oh) != ESP_OK) { g_oBegun = false; if (p) esp_partition_erase_range(p, 0, 4096); upFail("Immagine incompleta o non valida"); return; }
    g_oBegun = false;
    if (esp_ota_set_boot_partition(p) != ESP_OK) { upFail("Non riesco a impostare l'avvio"); return; }
    shaStop();
    g_fwMark = 1;                                       // il marchio c'era (rcFinalErr)
    Serial.printf("[recovery] controlli superati, SHA-256 %s\n", g_upSha.c_str());
  } else if (u.status == UPLOAD_FILE_ABORTED) {
    upFail("Caricamento interrotto");
  }
}

static void startNet() {
  g_code = String((unsigned long)(10000000UL + (esp_random() % 90000000UL)));   // 8 cifre
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  applyCountry();
  WiFi.setHostname(rc.host.c_str());
  g_net = true;
  if (rc.staOn && rc.ssid.length()) {
    if (!rc.dhcp && rc.ip.length()) {
      IPAddress ip, mk, gw, d1, d2;
      if (ip.fromString(rc.ip) && mk.fromString(rc.mask) && gw.fromString(rc.gw)) {
        if (!d1.fromString(rc.dns1)) d1 = gw;
        if (!d2.fromString(rc.dns2)) d2 = (uint32_t)0;
        WiFi.config(ip, gw, mk, d1, d2);
      }
    }
    Serial.printf("[recovery] mi collego alla rete di casa (fino a %d s)...\n", (int)(STA_WAIT_MS / 1000));
    WiFi.begin(rc.ssid.c_str(), rc.pass.c_str());
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < STA_WAIT_MS) { ledTick(); delay(50); }
  }
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    applyCountry();
    static const char* ab = "abcdefghijkmnpqrstuvwxyz23456789";
    g_apPass = "";
    for (int i = 0; i < 12; i++) g_apPass += ab[esp_random() % 32];
    g_ap = WiFi.softAP("VesevOS-recovery", g_apPass.c_str(), 1, 0, 2);   // canale 1, massimo 2 collegati
    if (!g_ap) Serial.println("[recovery] ERRORE: hotspot non partito");
  } else MDNS.begin(rc.host.c_str());
  static const char* hdrs[] = {"X-T", "X-Sha"};
  server.collectHeaders(hdrs, 2);
  server.on("/", HTTP_GET, hRoot);
  server.on("/api/ver", HTTP_GET, hVer);
  server.on("/api/start", HTTP_POST, hStart);
  server.on("/api/login", HTTP_POST, hLogin);
  server.on("/api/info", HTTP_POST, hInfo);
  server.on("/api/boot", HTTP_POST, hBoot);
  server.on("/upload", HTTP_POST, hUploadDone, hUpload);
  server.onNotFound([]() { jsonOut(404, jerr("Non trovato")); });
  server.begin();
  g_lastAct = millis();
  netInfo();
}

static const char* rstName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "accensione";
    case ESP_RST_EXT: return "tasto RESET";
    case ESP_RST_SW: return "riavvio da programma";
    case ESP_RST_PANIC: return "ERRORE GRAVE (panic)";
    case ESP_RST_INT_WDT: return "watchdog (interrupt)";
    case ESP_RST_TASK_WDT: return "watchdog (task)";
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_DEEPSLEEP: return "uscita dal sonno";
    case ESP_RST_BROWNOUT: return "CALO DI TENSIONE";
    case ESP_RST_USB: return "USB";
    default: return "altro";
  }
}
static const char* imgState(const esp_partition_t* p, int* err) {
  esp_ota_img_states_t st;
  *err = 0;
  if (!p) return "n/d";
  esp_err_t e = esp_ota_get_state_partition(p, &st);
  if (e != ESP_OK) { *err = (int)e; return "senza stato"; }
  switch (st) {
    case ESP_OTA_IMG_NEW: return "nuova";
    case ESP_OTA_IMG_PENDING_VERIFY: return "in prova (non confermata)";
    case ESP_OTA_IMG_VALID: return "valida";
    case ESP_OTA_IMG_INVALID: return "SCARTATA (invalida)";
    case ESP_OTA_IMG_ABORTED: return "ABBANDONATA (rollback)";
    default: return "indefinita";
  }
}
// Perche siamo qui? Scrive: motivo dell'ultimo riavvio, partizione in esecuzione, partizione scelta dal bootloader (otadata), ultima scartata, stato di app0.
static void diag() {
  const esp_partition_t *run = esp_ota_get_running_partition(), *bt = esp_ota_get_boot_partition(), *inv = esp_ota_get_last_invalid_partition();
  int er;
  const char* st = imgState(app0(), &er);
  Serial.printf("[recovery] diagnostica: riavvio per %s; in esecuzione 0x%X; scelta del bootloader 0x%X; ultima scartata %s; app0 %s (err %d); segno passaggio %s\n",
                rstName(esp_reset_reason()), run ? (unsigned)run->address : 0u, bt ? (unsigned)bt->address : 0u,
                inv ? "SI" : "nessuna", st, er, handFlag(false, false) ? "ACCESO" : "spento");
}

static void info() {
  char n[40] = "", v[40] = "";
  const esp_partition_t* p = app0();
  Serial.printf("\n[recovery %s] app0: ", REC_VERSION);
  if (p) Serial.printf("indirizzo 0x%X, dimensione %u KB, ", (unsigned)p->address, (unsigned)(p->size / 1024));
  else Serial.print("partizione NON trovata, ");
  if (app0Valid(n, sizeof n, v, sizeof v)) Serial.printf("firmware \"%s\" versione %s%s\n", n, v, app0Fw() ? "" : " - ATTENZIONE: NON e VesevOS (manca il marchio)");
  else Serial.println("nessun firmware valido");
  diag();
  netInfo();
}

void setup() {
  (void)*(volatile const char*)REC_MARK;             // tiene il marchio nel file anche dopo l'ottimizzazione
  Serial.begin(115200);
  pinMode(BOOT_PIN, INPUT_PULLUP);
  delay(1500);  // tempo per aprire il monitor seriale
  Serial.printf("\n\n=== VesevOS Recovery %s ===\n", REC_VERSION);
  char n[40], v[40];
  bool ok = app0Valid(n, sizeof n, v, sizeof v);
  bool notFw = ok && !app0Fw();                   // c'e un programma in app0 ma non e VesevOS (es. un altro recovery)
  if (notFw) ok = false;
  bool stay = stayFlag(false, false);
  bool boot = (digitalRead(BOOT_PIN) == LOW);
  bool back = handFlag(false, false);             // l'ultima volta siamo passati a VesevOS ma siamo tornati qui: non riprovare all'infinito
  info();
  if (back) handFlag(true, false);                // riprova al prossimo accensione/RESET
  if (ok && !stay && !boot && !back) {
    Serial.printf("[recovery] avvio %s tra %d s (tieni premuto BOOT all'accensione per restare qui)\n", n, HANDOVER_MS / 1000);
    delay(HANDOVER_MS);
    startApp();
    return;
  }
  Serial.println(notFw ? "[recovery] in app0 non c'e VesevOS (manca il marchio): resto qui, carica il firmware dalla pagina o con carica-firmware.sh" : back ? "[recovery] il passaggio a VesevOS non ha funzionato (siamo tornati qui): resto qui per non girare a vuoto. Premi RESET per riprovare, oppure carica un firmware dalla pagina" : stay ? "[recovery] richiesto da VesevOS: resto qui" : boot ? "[recovery] tasto BOOT premuto: resto qui" : "[recovery] niente da avviare: resto qui");
  loadConfig();
  g_ledOn = true; ledSet(LS_WAIT);
  startNet();
}

void loop() {
  static uint32_t last = 0;
  if (g_net) server.handleClient();
  ledTick();
  while (Serial.available()) {
    int c = Serial.read();
    if (c == 'i') info();
    else if (c == 'b') startApp();
    else if (c == 'r') { Serial.println("[recovery] riavvio"); delay(100); esp_restart(); }
  }
  if (g_tok.length() && millis() - g_tokT > TOKEN_MS) g_tok = "";
  if (g_net && millis() - g_lastAct > IDLE_MS) {
    char n[40], v[40];
    if (app0Valid(n, sizeof n, v, sizeof v) && app0Fw()) { Serial.println("[recovery] nessun accesso da 15 minuti: torno a VesevOS"); startApp(); }
    g_lastAct = millis();
  }
  if (millis() - last > 15000) { last = millis(); Serial.println("[recovery] in attesa (i = info, b = avvia VesevOS, r = riavvia)"); }
  delay(2);
}
