// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_backup.cpp
// Comando "vesevos-backup" (solo Admin): backup e ripristino della CONFIGURAZIONE, sempre CIFRATO con una frase.
//
//   vesevos-backup [status]        elenco dei backup in /backup, spazio libero e avvertenza (la configurazione ha password e chiavi)
//   vesevos-backup list            solo l'elenco
//   vesevos-backup create [nome]   crea /backup/<nome>.vbk (senza nome: backup-1, backup-2...). La frase si chiede due volte
//   vesevos-backup restore <nome>  chiede la frase, mostra QUANTE righe cambierebbero (mai i valori), chiede conferma, poi applica
//   vesevos-backup delete <nome>   cancella il file, dopo conferma
//
// FORMATO: lo stesso del backup con i segreti della pagina (cfgExport con i segreti, poi cryptSeal: riga "VOSENC1 ..."). Quindi un file creato
// qui si ripristina dalla pagina (Sistema > Backup > Ripristina, dopo averlo scaricato da Sistema > File) e viceversa (basta caricarlo in
// /backup con il nome terminato in .vbk). La configurazione in chiaro NON si scrive mai su file da qui.
//
// SEGRETI: la frase non sta mai nella riga (ne in cronologia, ne in registro): seriale = domanda senza eco; pagina = campo "pw" a parte.
// Una riga con la frase in coda viene rifiutata. Il testo della configurazione e la frase si cancellano dalla memoria appena usati.
// Del contenuto si stampano solo numeri e nomi di sezione, mai valori.
// Nomi: solo lettere, numeri, '-' e '_' (al massimo 24): niente '/' e niente '..', quindi niente percorsi. Al massimo 5 backup.
#include "vos_shell_int.h"
#include "vos_shell_ask.h"
#include "vos_shell_rep.h"
#include "../core/vos_files_stream.h"
#include "../security/vos_crypt.h"
#include "../drivers/vos_drv_task.h"
#include <ctype.h>
#include <string_view>

namespace {

const char* const kDir = "/backup";
const char* const kExt = ".vbk";
const size_t      kExtLen = 4;
const size_t      kNameMax = 24;
const int         kMaxFiles = 5;
const size_t      kFileMax = 46000;        // come il limite della pagina per il ripristino (/api/config/restore)
const size_t      kSpaceMargin = 8192;     // spazio da lasciare libero, come fsWriteText

// Cancella davvero (volatile: il compilatore non puo togliere la scrittura "inutile" prima di liberare la memoria)
void wipe(char* p, size_t n) {
  volatile char* v = p;
  while (n--) *v++ = 0;
}

void scrub(HeapBuf& h) {
  wipe(const_cast<char*>(h.c_str()), h.length());
  h.clear();
}

// ---- nomi ----
bool nameCharsOk(const char* n, size_t len) {
  if (len == 0 || len > kNameMax || !isalnum((unsigned char)n[0])) return false;
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)n[i];
    if (!isalnum(c) && c != '-' && c != '_') return false;
  }
  return true;
}

// Il nome scritto dall'utente (con o senza ".vbk") diventa il nome semplice in out. false = non valido.
bool takeName(const Word& w, char* out, size_t cap) {
  size_t n = w.length();
  if (w.cut()) return false;
  if (n > kExtLen && strcmp(w.c_str() + n - kExtLen, kExt) == 0) n -= kExtLen;
  if (n >= cap || !nameCharsOk(w.c_str(), n)) return false;
  memcpy(out, w.c_str(), n);
  out[n] = 0;
  Word low(out, n);
  return low.lower() != "yes";                       // "yes" in fondo alla riga e la conferma, non un nome
}

bool isBackupFile(const char* fname) {
  size_t n = strlen(fname);
  return n > kExtLen && strcmp(fname + n - kExtLen, kExt) == 0 && nameCharsOk(fname, n - kExtLen);
}

void pathOf(const char* name, char* out, size_t cap) { snprintf(out, cap, "%s/%s%s", kDir, name, kExt); }

// Il nome (terza parola) senza l'eventuale "yes" messo dalla conferma. Vuoto se manca.
Word nameWord(const char* line) {
  Word w = argAt(line, 2);
  if (askConfirmed(line) && w == "yes" && argAt(line, 3).empty()) return Word();
  return w;
}

// La frase non si scrive nella riga: dopo il nome puo esserci solo la conferma "yes".
bool noSecretInLine(const char* line, int idx, OutSink& o) {
  Word w = argAt(line, idx);
  if (w.empty() || (askConfirmed(line) && w == "yes" && argAt(line, idx + 1).empty())) return true;
  o.println(tr("La frase non si scrive nella riga: te la chiedo io, senza mostrarla"));
  return false;
}

// ---- elenco ----
// Conta i backup di /backup; se o c'e li stampa (nome e dimensione). Guarda al massimo il doppio del tetto di voci.
int listBackups(OutSink* o) {
  VosFile d = drvFsOpen(kDir, "r");
  if (!d || !d.isDirectory()) return 0;
  int n = 0;
  VosFile e = d.openNextFile();
  for (int seen = 0; e && seen < FS_DIR_MAX * 2; seen++, e = d.openNextFile()) {
    if (e.isDirectory() || !isBackupFile(e.name())) continue;
    if (o) {
      if (!n) o->printf("  %-*s %10s\r\n", (int)(kNameMax + kExtLen), tr("Nome"), tr("Dimens."));
      char sz[16];
      repBytes(e.size(), sz, sizeof sz);
      o->printf("  %-*s %10s\r\n", (int)(kNameMax + kExtLen), e.name(), sz);
    }
    n++;
  }
  return n;
}

void cmdStatus(OutSink& o, bool full) {
  o.println(tr("Backup della configurazione (cifrati con una frase) in /backup:"));
  int n = listBackups(&o);
  if (!n) o.println(tr("  nessun backup"));
  if (!full) return;
  char free_[16];
  repBytes(fsFreeBytes(), free_, sizeof free_);
  o.println(trf("Backup: %d su %d. Spazio libero nella memoria interna: %s", n, kMaxFiles, free_));
  o.println(tr("Attenzione: un backup contiene password e dati identificativi, cifrati con la tua frase. Se perdi la frase non si recupera. Tienilo dove decidi tu."));
  o.println(tr("Uso: vesevos-backup [list] | create [nome] | restore <nome> | delete <nome>"));
}

// ---- create ----
void autoName(char* out, size_t cap) {
  char path[48];
  for (int i = 1; i < 100; i++) {
    snprintf(out, cap, "backup-%d", i);
    pathOf(out, path, sizeof path);
    if (!drvFsExists(path)) return;
  }
}

// Si puo creare questo backup? (si ricontrolla quando arriva la frase: nel frattempo le cose possono cambiare)
bool createCheck(const char* path, OutSink& o) {
  if (drvFsExists(path)) { o.println(tr("Esiste gia un backup con questo nome: cancellalo o scegli un altro nome")); return false; }
  if (listBackups(nullptr) >= kMaxFiles) { o.println(trf("Ci sono gia %d backup: cancellane uno (vesevos-backup delete <nome>)", kMaxFiles)); return false; }
  if (const char* why = fsRoomFor(path)) { o.println(why); return false; }
  if (fsFreeBytes() < kSpaceMargin * 2) { o.println(tr("Spazio esaurito")); return false; }
  return true;
}

void cmdCreate(const char* line, OutSink& o) {
  Word w = nameWord(line);
  if (!noSecretInLine(line, 3, o)) return;
  char name[kNameMax + 1], path[48];
  if (w.empty()) autoName(name, sizeof name);
  else if (!takeName(w, name, sizeof name)) { o.println(tr("Nome non valido: da 1 a 24 caratteri, solo lettere, numeri, - e _ (niente percorsi)")); return; }
  pathOf(name, path, sizeof path);
  if (!createCheck(path, o)) return;
  const char* pw = askSecretGiven(o);
  if (pw) { shBackupSecret(SECRET_BACKUP_CREATE, name, pw, o); return; }
  o.println(tr("Il backup contiene password e dati identificativi: si cifra con una frase che scegli tu. Se la perdi non si recupera."));
  askSecret(o, SECRET_BACKUP_CREATE, name, 0, true);
}

// La frase c'e: si crea il file. Il testo in chiaro vive solo qui dentro e si cancella subito dopo la cifratura.
void createApply(const char* name, const char* pass, OutSink& o) {
  if (!*pass) { o.println(tr("Frase vuota: non creo niente")); return; }
  char path[48];
  pathOf(name, path, sizeof path);
  if (!createCheck(path, o)) return;
  ErrBuf err;
  if (!drvFsExists(kDir) && !fsMkdirs(kDir, err)) { o.println(err.c_str()); return; }
  HeapBuf plain(CFG_TEXT_MAX);
  if (!cfgExport(plain, true)) { scrub(plain); o.println(tr("Memoria insufficiente")); return; }
  HeapBuf sealed(CRYPT_OUT_MAX(plain.length()));
  bool ok = cryptSeal(plain.c_str(), pass, sealed, err);
  scrub(plain);
  if (!ok) { o.println(err.c_str()); return; }
  if (sealed.length() > kFileMax) { o.println(tr("La configurazione e troppo grande per un backup che la pagina possa ripristinare")); return; }
  if (sealed.length() + kSpaceMargin > fsFreeBytes()) { o.println(tr("Spazio esaurito")); return; }
  FsAtomicWriter wr;
  if (!wr.begin(path, false, kFileMax, err)) { o.println(err.c_str()); return; }
  const uint8_t* p = (const uint8_t*)sealed.c_str();
  for (size_t off = 0; off < sealed.length(); off += 256) {
    size_t n = sealed.length() - off < 256 ? sealed.length() - off : 256;
    if (!wr.write(p + off, n)) { wr.abort(); o.println(tr("Spazio esaurito")); return; }
    drvTaskPause();
  }
  size_t size = wr.written();
  if (!wr.commit(err)) { o.println(err.c_str()); return; }
  vlog("CFG: backup cifrato della configurazione creato dalla shell (%s, %u byte)", name, (unsigned)size);
  auditEvent(AUD_YELLOW, "", trf("backup cifrato della configurazione creato dalla shell: %s", name).c_str());
  char sz[16];
  repBytes(size, sz, sizeof sz);
  o.println(trf("Backup creato: %s (%s, cifrato)", path, sz));
  o.println(tr("Senza la frase non si apre. Si ripristina da qui (restore) o dalla pagina, dopo averlo scaricato da Sistema > File."));
}

// ---- restore ----
bool readFile(const char* path, HeapBuf& out, ErrBuf& err) {
  VosFile f = drvFsOpen(path, "r");
  if (!f || f.isDirectory()) { err = tr("Backup non trovato"); return false; }
  if (f.size() > kFileMax) { err = tr("File troppo grande per essere un backup"); return false; }
  uint8_t buf[256];
  int rounds = 0;
  while (f.available()) {
    size_t n = f.read(buf, sizeof buf);
    if (n == 0) break;
    if (!out.add((const char*)buf, n)) { out.clear(); err = tr("Memoria insufficiente"); return false; }
    if ((++rounds & 7) == 0) drvTaskPause();
  }
  return true;
}

// Legge e decifra il file: in plain c'e la configurazione in chiaro (da cancellare con scrub dopo l'uso).
bool openBackup(const char* path, const char* pass, HeapBuf& plain, ErrBuf& err) {
  HeapBuf sealed(kFileMax + 1);
  if (!readFile(path, sealed, err)) return false;
  if (!cryptIsSealed(sealed.c_str())) { err = tr("Il file non e un backup cifrato di VesevOS"); return false; }
  return cryptOpen(sealed.c_str(), pass, plain, err);
}

// Il nome della sezione di una riga "config tipo 'nome'": il nome tra apici, o il tipo
void sectionName(std::string_view line, char* out, size_t cap) {
  size_t a = line.find('\''), b = a == std::string_view::npos ? a : line.find('\'', a + 1);
  std::string_view n = b != std::string_view::npos ? line.substr(a + 1, b - a - 1) : line.substr(7);
  size_t k = 0;
  for (; k < n.size() && k < cap - 1 && n[k] != '\n' && n[k] != ' '; k++) out[k] = n[k];
  out[k] = 0;
}

void noteSection(char* list, size_t cap, const char* sec) {
  if (!sec[0] || strstr(list, sec)) return;
  size_t n = strlen(list);
  if (n + strlen(sec) + 3 < cap) snprintf(list + n, cap - n, "%s%s", n ? ", " : "", sec);
}

// Quante righe del backup non ci sono (uguali) nella configurazione di adesso: stessa idea del conteggio della pagina, ma PRIMA di applicare.
// Righe vuote e commenti non contano. In secs i nomi delle sezioni toccate. Non si copia nessun valore.
int diffLines(const HeapBuf& was, const HeapBuf& now, char* secs, size_t cap) {
  std::string_view old(was.c_str(), was.length()), nw(now.c_str(), now.length());
  char sec[32] = "";
  int changed = 0, seen = 0;
  secs[0] = 0;
  for (size_t i = 0; i < nw.size();) {
    size_t e = nw.find('\n', i);
    size_t end = e == std::string_view::npos ? nw.size() : e + 1;
    std::string_view line = nw.substr(i, end - i);
    i = end;
    if (line.size() <= 1 || line[0] == '#') continue;
    if (line.substr(0, 7) == "config ") sectionName(line, sec, sizeof sec);
    if (old.find(line) != std::string_view::npos) continue;
    changed++;
    noteSection(secs, cap, sec);
    if ((++seen & 15) == 0) drvTaskPause();
  }
  return changed;
}

void cmdRestore(const char* line, OutSink& o) {
  Word w = nameWord(line);
  if (w.empty()) { o.println(tr("Uso: vesevos-backup restore <nome>")); return; }
  if (!noSecretInLine(line, 3, o)) return;
  char name[kNameMax + 1], path[48];
  if (!takeName(w, name, sizeof name)) { o.println(tr("Nome non valido: da 1 a 24 caratteri, solo lettere, numeri, - e _ (niente percorsi)")); return; }
  pathOf(name, path, sizeof path);
  if (!drvFsExists(path)) { o.println(tr("Backup non trovato (elenco: vesevos-backup list)")); return; }
  SecretFor what = askConfirmed(line) ? SECRET_BACKUP_RESTORE_YES : SECRET_BACKUP_RESTORE;
  const char* pw = askSecretGiven(o);
  if (pw) shBackupSecret(what, name, pw, o);
  else askSecret(o, what, name, 0, false);
}

// La frase c'e. Prima volta (confirmed = false): decifra, mostra il riepilogo e chiede conferma. Seconda volta (la riga torna con " yes"
// e la frase si richiede): decifra di nuovo e applica.
void restoreApply(const char* name, const char* pass, bool confirmed, OutSink& o) {
  if (!*pass) { o.println(tr("Frase vuota: non ripristino niente")); return; }
  char path[48], secs[96];
  pathOf(name, path, sizeof path);
  ErrBuf err;
  HeapBuf plain(kFileMax);
  if (!openBackup(path, pass, plain, err)) { scrub(plain); o.println(err.c_str()); return; }
  HeapBuf cur(CFG_TEXT_MAX);
  if (!cfgExport(cur, true)) { scrub(plain); scrub(cur); o.println(tr("Memoria insufficiente")); return; }
  int changed = diffLines(cur, plain, secs, sizeof secs);
  o.println(trf("Il backup '%s' cambierebbe circa %d righe della configurazione.", name, changed));
  if (secs[0]) o.println(trf("Sezioni toccate: %s", secs));
  if (changed == 0) { scrub(plain); scrub(cur); o.println(tr("Niente da ripristinare: la configurazione e gia uguale")); return; }

  if (!confirmed) {
    scrub(plain); scrub(cur);
    char again[64];
    snprintf(again, sizeof again, "vesevos-backup restore %s", name);
    askConfirm(o, again, tr("Applico il backup? La configurazione attuale viene sostituita (anche password e utenti). Procedo?"));
    return;
  }

  bool ok = cfgImport(plain.c_str(), plain.length(), err);
  if (!ok) {                                         // a meta: si rimette com'era (stesso testo preso prima)
    ErrBuf back;
    cfgImport(cur.c_str(), cur.length(), back);
    o.println(trf("Ripristino non riuscito (%s): la configurazione e rimasta com'era", err.c_str()));
  }
  scrub(plain); scrub(cur);
  if (!ok) return;
  cfgSave(); ledApplyConfig(); netReconfigure();     // come il ripristino dalla pagina
  vlog("CFG: ripristino dalla shell (%d righe cambiate) dal backup %s", changed, name);
  auditEvent(AUD_YELLOW, "", trf("configurazione ripristinata dal backup %s dalla shell: %d righe", name, changed).c_str());
  o.println(trf("Ripristinato: %d righe cambiate", changed));
  o.println(tr("Se la rete o gli utenti sono cambiati puo servire rientrare; se qualcosa non risponde usa reboot."));
}

// ---- delete ----
void cmdDelete(const char* line, OutSink& o) {
  Word w = nameWord(line);
  if (w.empty()) { o.println(tr("Uso: vesevos-backup delete <nome>")); return; }
  char name[kNameMax + 1], path[48];
  if (!takeName(w, name, sizeof name)) { o.println(tr("Nome non valido: da 1 a 24 caratteri, solo lettere, numeri, - e _ (niente percorsi)")); return; }
  pathOf(name, path, sizeof path);
  if (!drvFsExists(path)) { o.println(tr("Backup non trovato (elenco: vesevos-backup list)")); return; }
  if (!askConfirmed(line)) { askConfirm(o, line, trf("Cancello il backup '%s'? Non si annulla. Procedo?", name)); return; }
  ErrBuf err;
  if (!fsRemove(path, err)) { o.println(err.c_str()); return; }
  vlog("CFG: backup %s cancellato dalla shell", name);
  o.println(trf("Backup cancellato: %s", name));
}

}  // namespace

void shx::shBackupSecret(SecretFor what, const char* name, const char* pass, OutSink& o) {
  if (what == SECRET_BACKUP_CREATE) createApply(name, pass, o);
  else restoreApply(name, pass, what == SECRET_BACKUP_RESTORE_YES, o);
}

void shx::shBackup(const char* line, OutSink& o, int role) {
  if (role != ROLE_ADMIN) { o.println(tr("Serve il livello Admin")); return; }
  Word sub = argAt(line, 1);
  sub.lower();
  if (sub.empty() || sub == "status") cmdStatus(o, true);
  else if (sub == "list") cmdStatus(o, false);
  else if (sub == "create") cmdCreate(line, o);
  else if (sub == "restore") cmdRestore(line, o);
  else if (sub == "delete") cmdDelete(line, o);
  else o.println(tr("Uso: vesevos-backup [list] | create [nome] | restore <nome> | delete <nome>"));
}
