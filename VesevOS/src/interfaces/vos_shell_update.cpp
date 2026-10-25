// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_update.cpp
// Comando "vesevos-update" (solo Admin): stato e avvio dell'aggiornamento TRAMITE RECOVERY. Nessuna logica nuova: usa le stesse funzioni
// delle rotte della pagina (core/vos_updorder: updUrlAllowed, updOrderNet, updOrderFile; vedi interfaces/vos_web_upd.cpp), quindi stessa
// lista bianca degli indirizzi, stessi controlli e stesso ordine scritto nella NVS. Qui non parte nessuna connessione verso l'esterno:
// e il recovery a caricare o scaricare il file, e solo se l'Admin lo ordina e lo conferma.
//
//   vesevos-update [status]                      versione del firmware e del recovery, partizioni, esito dell'ultimo ordine
//   vesevos-update file [ip-del-computer]        modo 2: la scheda va nel recovery e accetta UN file dal computer (gettone monouso, 15 minuti)
//   vesevos-update net <url-https> <sha256> [versione]   modo 1: il recovery scarica da un indirizzo https consentito e controlla SHA-256
//
// Dalla pagina l'IP del computer e quello della richiesta; dalla seriale va scritto (il gettone si lega a quel computer).
// Il GETTONE del modo 2 si stampa UNA volta sola, nella risposta a chi ha chiesto: non va mai nel registro, nell'audit o nella cronologia.
// L'indirizzo del modo 1 non si scrive in nessun registro (solo versione e le prime 12 cifre dello SHA-256, come la rotta della pagina).
// Nota: una riga non supera i 200 caratteri, quindi l'indirizzo del modo 1 puo essere lungo circa 110 caratteri.
#include "vos_shell_int.h"
#include "vos_shell_ask.h"
#include "vos_shell_rep.h"
#include "../core/vos_updorder.h"
#include <ctype.h>

namespace {

const uint32_t kBootDelayMs = 800;      // come le rotte: il tempo di far arrivare la risposta prima del riavvio

// Il nome di chi ha chiesto, per il registro (parole fisse)
const char* whoName(OutSink& o) { return serIsOut(o) ? "seriale" : "pagina"; }

bool isHex64(const char* s) {
  if (strlen(s) != 64) return false;
  for (; *s; s++) if (!isxdigit((unsigned char)*s)) return false;
  return true;
}

// La parola idx senza l'eventuale "yes" messo dalla conferma (in coda alla riga)
Word argNoYes(const char* line, int idx) {
  Word w = argAt(line, idx);
  if (askConfirmed(line) && w == "yes" && argAt(line, idx + 1).empty()) return Word();
  return w;
}

// Copia la prossima parola di p (separata da spazi) in out; false se manca o non ci sta. p avanza.
bool nextToken(const char*& p, char* out, size_t cap) {
  while (*p == ' ') p++;
  size_t n = 0;
  while (p[n] && p[n] != ' ') n++;
  if (n == 0 || n >= cap) return false;
  memcpy(out, p, n);
  out[n] = 0;
  p += n;
  return true;
}

// ---- status ----
void cmdStatus(OutSink& o) {
  HeapBuf h(1024);
  char run[64], next[64], buf[48];
  repRow(h, TRK("Firmware"), VOS_VERSION);
  if (drvRecoveryPresent()) { const char* v = recVersion(); repRow(h, TRK("Recovery"), v[0] ? v : "?"); }
  else repRow(h, TRK("Recovery"), tr("non presente (tabella partizioni vecchia)"));
  drvOtaRunningTo(run, sizeof run);
  drvOtaNextTo(next, sizeof next);
  repRow(h, TRK("In uso"), run[0] ? run : "?");
  repRow(h, TRK("Prossimo in"), next[0] ? next : "?");
  snprintf(buf, sizeof buf, "%lu KB", (unsigned long)(drvFreeSketchSpace() / 1024));
  repRow(h, TRK("Spazio OTA"), buf);
  const UpdOutcome& u = updOutcome();
  if (u.res) {
    repRow(h, TRK("Ultimo ordine"), updResText(u.res));
    if (u.ver[0]) repRow(h, TRK("Versione"), u.ver);
    if (u.msg[0]) repRaw(h, tr("Dal recovery"), u.msg);               // testo del recovery (inglese): si mostra e basta
    updOutcomeSeen();
  } else repRow(h, TRK("Ultimo ordine"), tr("nessun esito da mostrare"));
  repFlush(o, h);
  o.println(tr("Aggiornare: vesevos-update file (dal computer) oppure vesevos-update net <url-https> <sha256>. Non cerca niente in rete da solo."));
}

// ---- file (modo 2) ----
void cmdFile(const char* line, OutSink& o) {
  if (!drvRecoveryPresent()) { o.println(tr("Recovery non presente: la scheda ha ancora la tabella partizioni vecchia.")); return; }
  uint32_t ip = serIsOut(o) ? 0 : shellClientIp();                    // dalla pagina: il computer che scrive; dalla seriale: quello indicato
  if (!ip) {
    Word w = argNoYes(line, 2);
    if (w.empty() || !ipParse(w.c_str(), ip)) { o.println(tr("Dalla seriale scrivi l'IP del computer che carichera il file: vesevos-update file 192.168.1.20")); return; }
  }
  if (!askConfirmed(line)) {
    o.println(tr("La scheda si riavvia nel recovery e apre una pagina per caricare UN file di firmware."));
    o.println(tr("Attenzione: il recovery lavora in HTTP (senza cifratura) sulla rete locale, e l'indirizzo cambia."));
    askConfirm(o, line, tr("Riavvio nel recovery per aggiornare il firmware. Procedo?"));
    return;
  }
  char tok[33];
  ErrBuf err;
  if (!updOrderFile(ip, tok, err)) { o.println(err.c_str()); return; }
  vlog("SISTEMA: riavvio nel recovery per caricare un file (%s)", whoName(o));                            // il gettone non va mai nel registro
  auditEvent(AUD_INFO, "", trf("aggiornamento tramite recovery (file dal computer) richiesto dalla shell (%s)", whoName(o)).c_str());
  char addr[16];
  netIpTo(addr, sizeof addr);
  o.println(tr("Ordine scritto: la scheda si riavvia nel recovery tra un attimo."));
  o.println(trf("Apri dal computer: http://%s/#g=%s", addr, tok));
  o.println(tr("Codice monouso valido 15 minuti, legato a questo computer: non condividerlo. Non verra mostrato di nuovo."));
  memset(tok, 0, sizeof tok);
  sysRebootLater("recovery per aggiornamento", kBootDelayMs);
}

// ---- net (modo 1) ----
void cmdNet(const char* line, OutSink& o) {
  if (!drvRecoveryPresent()) { o.println(tr("Recovery non presente: la scheda ha ancora la tabella partizioni vecchia.")); return; }
  char url[200], sha[65], ver[24] = "";
  const char* p = restFrom(line, 2);
  if (!nextToken(p, url, sizeof url) || !nextToken(p, sha, sizeof sha)) {
    o.println(tr("Uso: vesevos-update net <url-https> <sha256> [versione]   (solo indirizzi consentiti; lo SHA-256 sono 64 cifre esadecimali)"));
    return;
  }
  if (nextToken(p, ver, sizeof ver) && !strcmp(ver, "yes") && askConfirmed(line)) ver[0] = 0;   // "yes" e la conferma, non la versione
  ErrBuf err;
  if (!updUrlAllowed(url, err)) { o.println(err.c_str()); return; }
  if (!isHex64(sha)) { o.println(tr("L'impronta SHA-256 deve avere 64 cifre esadecimali")); return; }
  if (!askConfirmed(line)) {
    o.println(tr("La scheda si riavvia nel recovery, che scarica il file dall'indirizzo e controlla lunghezza e SHA-256."));
    o.println(tr("Attenzione: il recovery sovrascrive il firmware (app0) prima di verificare lo SHA-256 completo; la sua pagina e in HTTP sulla rete locale."));
    askConfirm(o, line, tr("Riavvio nel recovery per scaricare l'aggiornamento. Procedo?"));
    return;
  }
  if (!updOrderNet(url, sha, ver, 0, err)) { o.println(err.c_str()); return; }
  vlog("SISTEMA: riavvio nel recovery per scaricare l'aggiornamento (%s)", whoName(o));                   // l'indirizzo non va nel registro
  auditEvent(AUD_INFO, "", trf("aggiornamento tramite recovery (rete) richiesto dalla shell (%s): versione %s, SHA-256 %.12s", whoName(o), ver[0] ? ver : "?", sha).c_str());
  char addr[16];
  netIpTo(addr, sizeof addr);
  o.println(trf("Ordine scritto: la scheda si riavvia nel recovery (poi http://%s) e scarica il file. Esito: vesevos-update status", addr));
  memset(url, 0, sizeof url);
  sysRebootLater("recovery per aggiornamento", kBootDelayMs);
}

}  // namespace

void shx::shUpdate(const char* line, OutSink& o, int role) {
  if (role != ROLE_ADMIN) { o.println(tr("Serve il livello Admin")); return; }
  Word sub = argAt(line, 1);
  sub.lower();
  if (sub.empty() || sub == "status") cmdStatus(o);
  else if (sub == "file") cmdFile(line, o);
  else if (sub == "net") cmdNet(line, o);
  else o.println(tr("Uso: vesevos-update [status] | file [ip-del-computer] | net <url-https> <sha256> [versione]"));
}
