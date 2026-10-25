// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_updorder.cpp
#include "vos_updorder.h"
#include "vos_log.h"
#include "vos_i18n.h"
#include "vos_audit.h"
#include "vos_util.h"
#include "../drivers/vos_drv_nvs.h"
#include "../drivers/vos_drv_ota.h"
#include "../drivers/vos_drv_fs.h"
#include "../drivers/vos_drv_sys.h"

static const char* NS = "vosupd";
static const char* OFFICIAL_PREFIX = "https://github.com/paolellad79/VesevOS/releases/";
static const char* PREFIX_FILE = "/updurl.txt";
static const size_t URL_MAX = 200;              // con il terminatore, come dice il contratto

static UpdOutcome g_out;

// ---- lista bianca ----
static bool startsWith(const char* s, const char* prefix) { return strncmp(s, prefix, strlen(prefix)) == 0; }

// Un prefisso del file e valido se e https://, senza spazi, e finisce con "/" (cosi il nome del server non si puo allungare)
static bool prefixOk(const char* p, size_t n) {
  if (n < 12 || n >= URL_MAX || !startsWith(p, "https://") || p[n - 1] != '/') return false;
  for (size_t i = 0; i < n; i++) if ((unsigned char)p[i] <= 32 || (unsigned char)p[i] > 126 || p[i] == '@') return false;
  return true;
}

// true se url inizia con uno dei prefissi scritti in /updurl.txt
static bool matchesFilePrefix(const char* url) {
  if (!drvFsExists(PREFIX_FILE)) return false;
  VosFile f = drvFsOpen(PREFIX_FILE, "r");
  if (!f) return false;
  char buf[1024]; int n = f.read((uint8_t*)buf, sizeof buf - 1);
  f.close();
  if (n <= 0) return false;
  buf[n] = 0;
  for (char* line = strtok(buf, "\r\n"); line; line = strtok(nullptr, "\r\n")) {
    size_t len = strlen(line);
    while (len && line[len - 1] == ' ') line[--len] = 0;
    if (prefixOk(line, len) && startsWith(url, line)) return true;
  }
  return false;
}

bool updUrlAllowed(const char* url, ErrBuf& err) {
  size_t n = url ? strlen(url) : 0;
  if (!n) { err = tr("Indirizzo mancante"); return false; }
  if (n >= URL_MAX) { err = tr("Indirizzo troppo lungo (massimo 199 caratteri)"); return false; }
  if (!startsWith(url, "https://")) { err = tr("L'indirizzo deve iniziare con https://"); return false; }
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)url[i];
    if (c <= 32 || c > 126) { err = tr("Indirizzo non valido: solo caratteri ASCII visibili, niente spazi"); return false; }
    if (c == '@') { err = tr("Indirizzo non valido: niente nome utente e password dentro l'indirizzo"); return false; }
  }
  if (strstr(url, "..") || strchr(url, '\\')) { err = tr("Indirizzo non valido"); return false; }
  if (startsWith(url, OFFICIAL_PREFIX) || matchesFilePrefix(url)) return true;
  err = tr("Indirizzo non consentito: sono ammessi solo i file ufficiali di VesevOS su GitHub");
  return false;
}

// ---- scrittura dell'ordine ----
static bool hexVal(char c, uint8_t& v) {
  if (c >= '0' && c <= '9') v = c - '0';
  else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
  else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
  else return false;
  return true;
}

static bool hexTo32(const char* s, uint8_t out[32]) {
  if (!s || strlen(s) != 64) return false;
  for (int i = 0; i < 32; i++) {
    uint8_t a, b;
    if (!hexVal(s[i * 2], a) || !hexVal(s[i * 2 + 1], b)) return false;
    out[i] = (uint8_t)(a << 4 | b);
  }
  return true;
}

static bool verOk(const char* v) {
  size_t n = strlen(v);
  if (n > 23) return false;
  for (size_t i = 0; i < n; i++) if (!isalnum((unsigned char)v[i]) && v[i] != '.' && v[i] != '-' && v[i] != '_') return false;
  return true;
}

// Cancella l'ordine (se il passaggio al recovery non e riuscito)
static void orderCancel() {
  DrvNvs n;
  if (n.begin(NS, false)) { n.putUChar("mode", 0); n.clear(); }
}

// Tutte le chiavi del modo, "mode" per ultima; poi il recovery al prossimo avvio. Prima si cancella tutto, vecchio esito compreso.
static bool commitOrder(DrvNvs& n, uint8_t mode, ErrBuf& err) {
  n.putUChar("mode", mode);
  n.end();
  if (!drvRecoveryPresent()) { orderCancel(); err = tr("Recovery non presente: la scheda ha ancora la tabella partizioni vecchia."); return false; }
  if (!drvRecoveryEnter()) { orderCancel(); err = tr("Non riesco a passare al recovery."); return false; }
  return true;
}

bool updOrderNet(const char* url, const char* sha, const char* ver, uint32_t size, ErrBuf& err) {
  if (!updUrlAllowed(url, err)) return false;
  uint8_t h[32];
  if (!hexTo32(sha, h)) { err = tr("L'impronta SHA-256 deve avere 64 cifre esadecimali"); return false; }
  if (!ver) ver = "";
  if (!verOk(ver)) { err = tr("Versione non valida"); return false; }
  uint32_t space = drvFreeSketchSpace();                      // dimensione di app0
  if (!space || size > space) { err = tr("Dimensione non valida"); return false; }
  DrvNvs n;
  if (!n.begin(NS, false)) { err = tr("Memoria di configurazione non disponibile"); return false; }
  n.clear();
  n.putString("url", url);
  n.putBlob("sha", h, sizeof h);
  if (ver[0]) n.putString("ver", ver);
  n.putUInt("size", size);
  n.putUChar("exp", 30);                                  // minuti dall'avvio del recovery: rete, ora e scarico possono durare piu del solito
  return commitOrder(n, 1, err);
}

bool updOrderFile(uint32_t clientIp, char tokenHex[33], ErrBuf& err) {
  if (!clientIp) { err = tr("Indirizzo del computer non riconosciuto"); return false; }
  uint8_t tk[16];
  drvRandomFill(tk, sizeof tk);
  uint32_t cip = __builtin_bswap32(clientIp);              // il contratto vuole esp_ip4_addr_t.addr (ordine di rete)
  DrvNvs n;
  if (!n.begin(NS, false)) { err = tr("Memoria di configurazione non disponibile"); return false; }
  n.clear();
  n.putBlob("token", tk, sizeof tk);
  n.putUInt("cip", cip);
  n.putUChar("exp", 15);
  if (!commitOrder(n, 2, err)) return false;
  static const char* hx = "0123456789abcdef";
  for (int i = 0; i < 16; i++) { tokenHex[i * 2] = hx[tk[i] >> 4]; tokenHex[i * 2 + 1] = hx[tk[i] & 15]; }
  tokenHex[32] = 0;
  memset(tk, 0, sizeof tk);
  return true;
}

// ---- esito ----
const char* updResText(int res) {
  switch (res) {
    case UPD_OK: return tr("Aggiornamento installato");
    case UPD_E_RETE: return tr("Il recovery non ha trovato la rete");
    case UPD_E_TLS: return tr("Connessione sicura non riuscita (certificato o ora non disponibile)");
    case UPD_E_SHA: return tr("L'impronta SHA-256 del file scaricato non corrisponde");
    case UPD_E_DIM: return tr("File di dimensione o tipo sbagliato (non e un firmware VesevOS valido)");
    case UPD_E_SCRIVI: return tr("Errore nello scrivere o nell'avviare il nuovo firmware");
    case UPD_E_SCADUTO: return tr("Tempo scaduto: l'ordine non e stato eseguito");
    case UPD_E_ORDINE: return tr("Il recovery ha rifiutato l'ordine (dati non validi)");
    case UPD_E_ANNULLATO: return tr("Aggiornamento annullato o interrotto");
    default: return tr("Esito sconosciuto");
  }
}

void updOutcomeBoot() {
  memset(&g_out, 0, sizeof g_out);
  DrvNvs n;
  if (!n.begin(NS, false)) return;
  uint8_t res = n.getUChar("res", 0);
  if (res == 0) {
    if (n.getUChar("mode", 0)) { vlog("AGGIORNAMENTO: ordine rimasto nella memoria, il recovery non lo ha eseguito"); n.putUChar("mode", 0); n.clear(); }
    return;
  }
  if (res > UPD_E_ANNULLATO) res = UPD_E_ORDINE;
  g_out.res = res; g_out.fresh = true;
  char tmp[44];
  if (n.getString("rmsg", tmp, sizeof tmp)) cleanAscii(tmp, g_out.msg, sizeof g_out.msg);
  if (n.getString("rver", tmp, sizeof tmp)) cleanAscii(tmp, g_out.ver, sizeof g_out.ver);
  n.clear();                                              // esito letto: si cancella (anche eventuali resti dell'ordine)
  n.end();
  vlog("AGGIORNAMENTO: esito del recovery %d (%s) versione %s motivo %s", (int)res, updResText(res), g_out.ver[0] ? g_out.ver : "?", g_out.msg[0] ? g_out.msg : "-");
  auditEvent(res == UPD_OK ? AUD_INFO : AUD_YELLOW, "", trf("aggiornamento tramite recovery: esito %d, versione %s, %s", (int)res, g_out.ver[0] ? g_out.ver : "?", g_out.msg[0] ? g_out.msg : "-").c_str());
}

const UpdOutcome& updOutcome() { return g_out; }
void updOutcomeSeen() { g_out.fresh = false; }
