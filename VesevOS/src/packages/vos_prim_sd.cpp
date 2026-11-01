// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_prim_sd.cpp
// STRATO 6 (packages): gruppo "sd" del registro delle primitive (vedi vos_prim.h). Stessi nomi e stessa semantica delle funzioni Berry
// (vos_berry_sd.cpp, vos_berry_files.cpp), con firme in C semplice. ELF: simboli vos_sd_status, vos_sd_read, vos_sd_write, vos_sd_mount,
// vos_sd_eject, vos_sd_format, vos_sd_rm, vos_sd_mkdir, vos_listdir. I file veri li tocca devices/vos_sd_io.cpp.
//
// DIFFERENZE DA BERRY (scelte piu sicure, documentate in piano-integrazione-elf.md, sezione 4.2)
//   - Gli errori sono codici negativi VOS_PRIM_E_* (Berry alza un'eccezione o torna nil).
//   - Chiavi, certificati e copie .vbk (pathIsSecret) e file di sistema NON si leggono ne si toccano MAI, nemmeno se chi lancia e l'Admin
//     (Berry li lascia leggere all'Admin). Un file nascosto si comporta come se non esistesse.
//   - listdir sta qui (in Berry e nel gruppo files), vede SOLO la microSD, e mostra al massimo LS_PAGINA voci per chiamata.
//   - sd_status non restituisce una mappa ma 4 numeri e, se serve, il tipo di file system in un buffer del modulo.
// PERMESSI (decisi dal risolutore al caricamento): sd_status, sd_read e listdir da Operatore in su; il resto solo se chi lancia e l'Admin.
// REGOLE: percorso copiato con tetto, pathResolve (".." risolto), deve stare sotto /media/sd, poi sdPathOk. Blocchi di al massimo SD_IO_MAX byte.
#include "vos_prim.h"

#if VOS_WITH_ELF && VOS_WITH_SD
#include <stdio.h>
#include <string.h>
#include "../core/vos_files.h"
#include "../core/vos_log.h"
#include "../core/vos_pathres.h"
#include "../core/vos_paths.h"
#include "../devices/vos_sd.h"
#include "../devices/vos_vol.h"

#define PATH_IN_MAX 128                  // il testo del modulo, prima della normalizzazione
#define LS_PAGINA   8                    // voci per chiamata di listdir
#define LS_RIGA_MAX (1 + 1 + 10 + 1 + VOL_NAME_MAX + 1)   // "D 4294967295 nome\n"
#define LS_OUT_MIN  (LS_PAGINA * LS_RIGA_MAX + 1)         // buffer minimo del modulo per una pagina intera + zero finale

namespace {

struct Percorso {
  char        pulito[FS_PATH_MAX + 1];   // percorso intero "/media/sd/..."
  const char* rel;                       // parte sulla scheda "/dir/file" (dentro pulito)
};

// Testo del modulo -> percorso sulla microSD controllato. 0 oppure VOS_PRIM_E_PERCORSO.
int risolvi(const char* in, Percorso& p) {
  char grezzo[PATH_IN_MAX];
  char err[SD_ERR_MAX];
  if (!vos_prim_copia_testo(in, grezzo, sizeof(grezzo))) return VOS_PRIM_E_PERCORSO;
  if (!pathResolve("/", grezzo, p.pulito, sizeof(p.pulito))) return VOS_PRIM_E_PERCORSO;
  p.rel = pathSdRel(p.pulito);
  if (!p.rel) return VOS_PRIM_E_PERCORSO;
  if (!sdPathOk(p.rel, err, sizeof(err))) return VOS_PRIM_E_PERCORSO;
  return 0;
}

// Per scrivere o cancellare: ne i file di sistema ne i segreti.
bool nascosto(const Percorso& p) { return pathIsSystemFile(p.pulito) || pathIsSecret(p.pulito); }

int32_t inKb(uint32_t v) { return v > 0x7FFFFFFFu ? 0x7FFFFFFF : (int32_t)v; }

// valori[0] = stato (1 montata, 2 non montata, 3 senza scheda, 4 formato non supportato; 0 = nessuna periferica "sd"), [1] = totale KB,
// [2] = usato KB, [3] = kHz. Con n >= 6 anche [4] = 1 se si sta formattando e [5] = secondi trascorsi dall'inizio. fs (facoltativo) = tipo di file system,
// vuoto se non montata. Ritorna quanti valori ha scritto (4 o 6).
int primSdStatus(int32_t* valori, int n, char* fs, int fs_max) {
  if (!valori || n < 4 || (fs && fs_max < 1)) return VOS_PRIM_E_ARG;
  SdVolume v;
  if (!sdVolume(v)) memset(&v, 0, sizeof(v));
  valori[0] = (int32_t)v.state;
  valori[1] = inKb(v.totalKb);
  valori[2] = inKb(v.usedKb);
  valori[3] = inKb(v.khz);
  if (fs) snprintf(fs, (size_t)fs_max, "%s", v.fs);
  if (n < 6) return 4;
  SdFmtInfo f;
  sdFormatInfo(f);
  valori[4] = f.running ? 1 : 0;
  valori[5] = (int32_t)f.secs;
  return 6;
}

// Legge al massimo n byte (n <= 1024) dalla posizione off nel buffer del modulo. Ritorna i byte letti (0 = oltre la fine) o un errore.
int primSdRead(const char* percorso, int off, void* buf, int n) {
  if (!buf || off < 0 || n < 0) return VOS_PRIM_E_ARG;
  if (n > SD_IO_MAX) return VOS_PRIM_E_GRANDE;
  Percorso p;
  int e = risolvi(percorso, p);
  if (e) return e;
  if (nascosto(p)) return VOS_PRIM_E_NONTROVATO;
  if (!sdUp()) return VOS_PRIM_E_NOSD;
  char err[SD_ERR_MAX];
  int got = sdReadAt(p.rel, (uint32_t)off, (uint8_t*)buf, (size_t)n, err, sizeof(err));
  return got < 0 ? VOS_PRIM_E_NONTROVATO : got;
}

// Scrive n byte (n <= 1024) dal buffer del modulo; append != 0 = in coda. Ritorna 0 o un errore.
int primSdWrite(const char* percorso, const void* dati, int n, int append) {
  if (!dati || n < 0) return VOS_PRIM_E_ARG;
  if (n > SD_IO_MAX) return VOS_PRIM_E_GRANDE;
  Percorso p;
  int e = risolvi(percorso, p);
  if (e) return e;
  if (nascosto(p)) return VOS_PRIM_E_VIETATO;
  if (!sdUp()) return VOS_PRIM_E_NOSD;
  char err[SD_ERR_MAX];
  return sdWriteBytes(p.rel, (const uint8_t*)dati, (size_t)n, append != 0, err, sizeof(err)) ? 0 : VOS_PRIM_E_IO;
}

int primSdMount() {
  if (!sdConfigured()) return VOS_PRIM_E_NOSD;
  char err[SD_ERR_MAX];
  if (sdMount(err, sizeof(err))) return 0;
  vlog("ELF: sd_mount: %s", err);
  return VOS_PRIM_E_IO;
}

// Smonta come sd_eject di Berry: senza conferma, ma rifiuta se la scheda e in uso.
int primSdEject() {
  if (sdInUse()) return VOS_PRIM_E_OCCUPATO;
  char err[SD_ERR_MAX];
  if (sdUnmount(err, sizeof(err))) return 0;
  vlog("ELF: sd_eject: %s", err);
  return VOS_PRIM_E_IO;
}

// Avvia la formattazione della microSD in FAT32 (CANCELLA TUTTO): serve la parola di conferma, come sd_format("FORMATTA") di Berry.
// Torna subito (0 = avviata, task a parte); la fine si vede dallo stato della scheda.
int primSdFormat(const char* parola) {
  if (!parola || strcmp(parola, SD_FORMAT_WORD)) return VOS_PRIM_E_ARG;
  if (!sdConfigured()) return VOS_PRIM_E_NOSD;
  if (sdInUse() || sdFormatting()) return VOS_PRIM_E_OCCUPATO;
  char err[SD_ERR_MAX];
  if (sdFormatStart(err, sizeof(err))) return 0;
  vlog("ELF: sd_format: %s", err);
  return VOS_PRIM_E_IO;
}

int primSdRm(const char* percorso) {
  Percorso p;
  int e = risolvi(percorso, p);
  if (e) return e;
  if (nascosto(p)) return VOS_PRIM_E_VIETATO;
  if (!sdUp()) return VOS_PRIM_E_NOSD;
  char err[SD_ERR_MAX];
  return sdRemove(p.rel, err, sizeof(err)) ? 0 : VOS_PRIM_E_IO;
}

int primSdMkdir(const char* percorso) {
  Percorso p;
  int e = risolvi(percorso, p);
  if (e) return e;
  if (nascosto(p)) return VOS_PRIM_E_VIETATO;
  if (!sdUp()) return VOS_PRIM_E_NOSD;
  char err[SD_ERR_MAX];
  return sdMakeDir(p.rel, err, sizeof(err)) ? 0 : VOS_PRIM_E_IO;
}

int codiceDaVol(VolErr e) {
  switch (e) {
    case VOL_NOT_FOUND: return VOS_PRIM_E_NONTROVATO;
    case VOL_NO_MEDIA:  return VOS_PRIM_E_NOSD;
    case VOL_BAD_PATH:
    case VOL_NOT_DIR:   return VOS_PRIM_E_PERCORSO;
    case VOL_BAD_ARG:   return VOS_PRIM_E_ARG;
    default:            return VOS_PRIM_E_IO;
  }
}

VolEntry g_voci[LS_PAGINA];              // una pagina alla volta (un modulo alla volta): niente sullo stack del modulo
volatile int g_inElenco;

// Una riga per voce: "D <byte> <nome>" (cartella) o "F <byte> <nome>". Ritorna false se non entra (non succede con LS_OUT_MIN).
bool scriviVoci(const VolEntry* v, int n, char* out, int out_max) {
  int pos = 0;
  for (int i = 0; i < n; i++) {
    int w = snprintf(out + pos, (size_t)(out_max - pos), "%c %u %s\n", v[i].dir ? 'D' : 'F', (unsigned)v[i].size, v[i].name);
    if (w < 0 || w >= out_max - pos) return false;
    pos += w;
  }
  out[pos] = 0;
  return true;
}

// Elenca la cartella (solo sulla microSD) a pagine: parte dalla voce indice, scrive nel buffer del modulo (almeno LS_OUT_MIN byte, chiuso con
// zero) e mette in *prossimo l'indice per continuare, oppure -1 se non ce ne sono altre. Ritorna il numero di voci scritte o un errore.
int primListdir(const char* percorso, int indice, char* out, int out_max, int* prossimo) {
  if (!out || !prossimo || indice < 0 || indice > VOL_START_MAX) return VOS_PRIM_E_ARG;
  if (out_max < LS_OUT_MIN) return VOS_PRIM_E_GRANDE;
  Percorso p;
  int e = risolvi(percorso, p);
  if (e) return e;
  if (__atomic_exchange_n(&g_inElenco, 1, __ATOMIC_ACQ_REL)) return VOS_PRIM_E_OCCUPATO;
  int conta = 0, letti = 0;
  bool altre = false;
  VolErr er = volList(p.pulito, ROLE_OPER, indice, g_voci, LS_PAGINA, conta, letti, altre);   // Operatore: chiavi e certificati nascosti
  int ris = codiceDaVol(er);
  if (er == VOL_OK) {
    if (scriviVoci(g_voci, conta, out, out_max)) { *prossimo = altre ? indice + letti : -1; ris = conta; }
    else ris = VOS_PRIM_E_GRANDE;
  }
  __atomic_store_n(&g_inElenco, 0, __ATOMIC_RELEASE);
  return ris;
}

const vos_prim_t kSd[] = {
  { "sd_status", (const void*)primSdStatus, "int vos_sd_status(int32_t *valori, int n, char *fs, int fs_max)", ROLE_OPER, 2 },
  { "sd_read",   (const void*)primSdRead,   "int vos_sd_read(const char *percorso, int offset, void *buf, int n)", ROLE_OPER, 2 },
  { "listdir",   (const void*)primListdir,  "int vos_listdir(const char *percorso, int indice, char *out, int out_max, int *prossimo)", ROLE_OPER, 2 },
  { "sd_write",  (const void*)primSdWrite,  "int vos_sd_write(const char *percorso, const void *dati, int n, int append)", ROLE_ADMIN, 2 },
  { "sd_mount",  (const void*)primSdMount,  "int vos_sd_mount(void)", ROLE_ADMIN, 2 },
  { "sd_eject",  (const void*)primSdEject,  "int vos_sd_eject(void)", ROLE_ADMIN, 2 },
  { "sd_format", (const void*)primSdFormat, "int vos_sd_format(const char *parola)", ROLE_ADMIN, 3 },
  { "sd_rm",     (const void*)primSdRm,     "int vos_sd_rm(const char *percorso)", ROLE_ADMIN, 2 },
  { "sd_mkdir",  (const void*)primSdMkdir,  "int vos_sd_mkdir(const char *percorso)", ROLE_ADMIN, 2 },
};

}  // namespace

void vos_prim_sd_registra() {
  vos_prim_registra("sd", kSd, sizeof(kSd) / sizeof(kSd[0]));
}
#endif
